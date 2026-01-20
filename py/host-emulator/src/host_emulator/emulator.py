#!/usr/bin/env python
"""Device emulator for embedded C++ applications."""

from __future__ import annotations

import json
import logging
import sys
import time
from pathlib import Path
from threading import Thread
from typing import Any, NoReturn

import zmq

from .common import UnhandledMessageError
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
        self._ready = False

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

    def run(self) -> None:
        """Main emulator thread - BIND first, then signal ready."""
        logger.debug("Starting emulator thread")
        try:
            if self.from_device_endpoint.startswith("ipc://"):
                socket_path = Path(self.from_device_endpoint.replace("ipc://", ""))
                try:
                    socket_path.unlink()
                    logger.debug("Removed stale socket file: %s", socket_path)
                except FileNotFoundError:
                    pass

            self.from_device_socket.bind(self.from_device_endpoint)
            logger.debug("Bound to %s", self.from_device_endpoint)

            self.running = True
            self._ready = True

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
        """Start emulator and wait until ready."""
        self.emulator_thread.start()

        timeout = 5.0
        start_time = time.time()
        while not self._ready:
            if time.time() - start_time > timeout:
                raise RuntimeError("Emulator failed to start within timeout")
            time.sleep(0.01)

        self.to_device_socket.connect(self.to_device_endpoint)
        logger.debug("Connected to %s", self.to_device_endpoint)

        time.sleep(0.05)

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
