#!/usr/bin/env python
"""Device emulator for embedded C++ applications."""

from __future__ import annotations

import json
import logging
import sys
from threading import Event, Thread
from typing import Any, NoReturn

import zmq

from .common import UnhandledMessageError
from .endpoint import EndpointLock, has_live_owner
from .i2c import I2C
from .pin import Pin, PinDirection, PinState
from .uart import Uart

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

if not logger.handlers:
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG)
    formatter = logging.Formatter("[%(levelname)s] %(name)s: %(message)s")
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)


class DeviceEmulator:
    """Main emulator class coordinating all peripheral emulation."""

    DEFAULT_FROM_DEVICE_ENDPOINT = "ipc:///tmp/device_emulator.ipc"
    DEFAULT_TO_DEVICE_ENDPOINT = "ipc:///tmp/emulator_device.ipc"

    def __init__(
        self,
        from_device_endpoint: str | None = None,
        to_device_endpoint: str | None = None,
    ) -> None:
        """Initialize the device emulator.

        Args:
            from_device_endpoint: ZMQ endpoint to bind for receiving from device.
                                 Default: "ipc:///tmp/device_emulator.ipc"
            to_device_endpoint: ZMQ endpoint to connect for sending to device.
                               Default: "ipc:///tmp/emulator_device.ipc"
        """
        self.from_device_endpoint = (
            from_device_endpoint or self.DEFAULT_FROM_DEVICE_ENDPOINT
        )
        self.to_device_endpoint = to_device_endpoint or self.DEFAULT_TO_DEVICE_ENDPOINT

        logger.info("Creating DeviceEmulator")
        logger.debug("  from_device: %s", self.from_device_endpoint)
        logger.debug("  to_device: %s", self.to_device_endpoint)

        self.running = False

        self.context: zmq.Context[zmq.Socket[bytes]] = zmq.Context()

        self.to_device_socket: zmq.Socket[bytes] = self.context.socket(zmq.PAIR)
        self.from_device_socket: zmq.Socket[bytes] = self.context.socket(zmq.PAIR)

        self.to_device_socket.setsockopt(zmq.LINGER, 0)
        self.to_device_socket.setsockopt(zmq.SNDTIMEO, 1000)
        self.from_device_socket.setsockopt(zmq.LINGER, 0)
        self.from_device_socket.setsockopt(zmq.RCVTIMEO, 500)

        self.led_1 = Pin("LED 1", PinDirection.OUT, PinState.Low, self.to_device_socket)
        self.led_2 = Pin("LED 2", PinDirection.OUT, PinState.Low, self.to_device_socket)
        self.button_1 = Pin(
            "Button 1", PinDirection.IN, PinState.Low, self.to_device_socket
        )
        self.pins = [self.led_1, self.led_2, self.button_1]

        self.uart_1 = Uart("UART 1", self.to_device_socket)
        self.uarts = [self.uart_1]

        self.i2c_1 = I2C("I2C 1")
        self.i2cs = [self.i2c_1]

        self.emulator_thread = Thread(target=self.run)
        self._endpoint_lock = EndpointLock()
        # Set once the bind phase has settled, either way. start() waits on
        # this and then reads _startup_error, so a failure surfaces immediately
        # with its real cause instead of as a timeout with a generic message.
        self._bind_settled = Event()
        self._startup_error: Exception | None = None

    def user_led1(self) -> Pin:
        return self.led_1

    def user_led2(self) -> Pin:
        return self.led_2

    def user_button1(self) -> Pin:
        return self.button_1

    def uart1(self) -> Uart:
        return self.uart_1

    def i2c1(self) -> I2C:
        return self.i2c_1

    def _bind(self) -> None:
        """Claim the receive endpoint and bind it.

        No stale-file cleanup here, despite what the previous version did.
        libzmq unlinks an ipc path before binding it, so a file left behind by a
        killed process was never a problem -- and the unconditional unlink that
        used to live here was itself the hazard: it would displace a *live*
        emulator and take its endpoint, with no error on either side.

        Two guards instead, in order. The lock is the guarantee: it is atomic,
        so no other process using it can slip between the check and the bind.
        The probe is the fallback for an owner that holds no lock.
        """
        endpoint = self.from_device_endpoint
        if not self._endpoint_lock.try_acquire(endpoint):
            msg = f"Endpoint {endpoint} is locked by another live process"
            raise RuntimeError(msg)
        if has_live_owner(endpoint):
            msg = f"Endpoint {endpoint} is served by another live process"
            raise RuntimeError(msg)

        self.from_device_socket.bind(endpoint)
        logger.debug("Bound to %s", endpoint)

    def run(self) -> None:
        """Main emulator thread: bind, publish the outcome, then serve."""
        logger.debug("Starting emulator thread")
        try:
            self._bind()
        except Exception as exc:  # Recorded here, re-raised by start().
            self._startup_error = exc
            logger.error("Emulator failed to bind: %s", exc)
            self.from_device_socket.close()
            self._endpoint_lock.release()
            return
        finally:
            # Publish on every path out of the bind phase. start() is blocked
            # on this; an unpublished outcome makes it wait out its whole
            # timeout for something that will never arrive.
            self._bind_settled.set()

        try:
            self.running = True

            while self.running:
                try:
                    message = self.from_device_socket.recv()

                    if not (message.startswith(b"{") and message.endswith(b"}")):
                        logger.warning("Received non-JSON message: %s", message)
                        continue

                    json_message: dict[str, Any] = json.loads(message)
                    object_type = json_message.get("object")

                    if object_type == "Pin":
                        self._handle_pin_message(json_message)
                    elif object_type == "Uart":
                        self._handle_uart_message(json_message)
                    elif object_type == "I2C":
                        self._handle_i2c_message(json_message)
                    else:
                        raise UnhandledMessageError(
                            f"Unknown object type: {object_type}"
                        )

                except zmq.Again:
                    if not self.running:
                        break
                    continue

        except Exception:
            logger.exception("Emulator thread error")
        finally:
            self.from_device_socket.close()
            # Released only once the socket is closed, so the endpoint is never
            # advertised as free while we still hold it.
            self._endpoint_lock.release()
            logger.debug("Emulator thread exiting")

    def _handle_pin_message(self, json_message: dict[str, Any]) -> None:
        """Handle a Pin message by dispatching to the appropriate pin."""
        for pin in self.pins:
            if response := pin.handle_message(json_message):
                self.from_device_socket.send_string(response)
                return
        raise UnhandledMessageError(f"Pin not found: {json_message.get('name')}")

    def _handle_uart_message(self, json_message: dict[str, Any]) -> None:
        """Handle a Uart message by dispatching to the appropriate uart."""
        for uart in self.uarts:
            if response := uart.handle_message(json_message):
                self.from_device_socket.send_string(response)
                return
        raise UnhandledMessageError(f"Uart not found: {json_message.get('name')}")

    def _handle_i2c_message(self, json_message: dict[str, Any]) -> None:
        """Handle an I2C message by dispatching to the appropriate i2c."""
        for i2c in self.i2cs:
            if response := i2c.handle_message(json_message):
                self.from_device_socket.send_string(response)
                return
        raise UnhandledMessageError(f"I2C not found: {json_message.get('name')}")

    def start(self) -> None:
        """Start the emulator, raising if it could not claim its endpoint.

        Waits for the bind outcome rather than for a duration, so the common
        case returns as soon as the socket is bound and the failure case
        reports why instead of timing out with a generic message.

        Raises:
            RuntimeError: If the endpoint is owned by another live process, or
                the emulator thread never reported a bind outcome.
        """
        self.emulator_thread.start()

        if not self._bind_settled.wait(timeout=5.0):
            raise RuntimeError("Emulator thread never reported a bind outcome")
        if self._startup_error is not None:
            raise RuntimeError(
                f"Emulator failed to start: {self._startup_error}"
            ) from self._startup_error

        self.to_device_socket.connect(self.to_device_endpoint)
        logger.debug("Connected to %s", self.to_device_endpoint)
        # No settling sleep here. connect() is asynchronous and the device may
        # not even have bound yet; libzmq retries in the background regardless.
        # PAIR blocks rather than drops, and SNDTIMEO bounds the wait, so the
        # sleep bought nothing. Test-side readiness is _wait_for_process_ready.

    def stop(self) -> None:
        """Stop emulator and clean up resources."""
        logger.info("Stopping emulator")
        self.running = False

        self.emulator_thread.join(timeout=2.0)

        self.to_device_socket.close()
        self.context.term()
        logger.info("Emulator stopped")

    def uart_initialized(self, name: str) -> bool:
        """Check if a UART with the given name exists."""
        return any(uart.name == name for uart in self.uarts)

    def get_uart_tx_data(self, name: str) -> list[int] | None:
        """Get data that was transmitted (sent) from the device to the emulator."""
        for uart in self.uarts:
            if uart.name == name:
                if len(uart.rx_buffer) > 0:
                    return list(uart.rx_buffer)
                return None
        return None

    def clear_uart_tx_data(self, name: str) -> bool:
        """Clear the TX buffer (data received from device)."""
        for uart in self.uarts:
            if uart.name == name:
                uart.rx_buffer.clear()
                return True
        return False

    def uart_send_to_device(self, name: str, data: bytes) -> dict[str, Any] | None:
        """Send data from emulator to device (simulating external UART input)."""
        for uart in self.uarts:
            if uart.name == name:
                return uart.send_data(data)
        return None

    def get_pin_state(self, name: str) -> PinState | None:
        """Get the current state of a pin."""
        for pin in self.pins:
            if pin.name == name:
                return pin.state
        return None


def main() -> NoReturn:
    emulator = DeviceEmulator()
    try:
        emulator.start()
        logger.info("Sending Hello")
        emulator.to_device_socket.send_string("Hello")
        logger.info("Waiting for reply")
        reply = emulator.to_device_socket.recv()
        logger.info("Received reply: %s", reply)
        pin_reply = emulator.user_button1().get_state()
        logger.info("Received pin reply: %s", pin_reply)
        while emulator.running:
            emulator.emulator_thread.join(0.5)
    except (KeyboardInterrupt, SystemExit):
        logger.info("Received keyboard interrupt")
        emulator.stop()
    sys.exit(0)


if __name__ == "__main__":
    main()
