"""Device emulator for embedded C++ applications."""

import json
import logging
import sys
from threading import Event, Thread
from typing import Any, NoReturn

import zmq

from .common import MessageType, Status, UnhandledMessageError
from .endpoint import EndpointLock, has_live_owner
from .i2c import I2C
from .peripheral import Peripheral
from .pin import Pin, PinDirection, PinState
from .uart import Uart

logger = logging.getLogger(__name__)


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

        self.led_1 = Pin(
            "LED 1", PinDirection.Output, PinState.Low, self.to_device_socket
        )
        self.led_2 = Pin(
            "LED 2", PinDirection.Output, PinState.Low, self.to_device_socket
        )
        self.button_1 = Pin(
            "Button 1", PinDirection.Input, PinState.Low, self.to_device_socket
        )
        self.uart_1 = Uart("UART 1", self.to_device_socket)
        self.i2c_1 = I2C("I2C 1")

        # Routing table: the `object` field of an incoming message selects the
        # candidate peripherals, the `name` field picks one of them.
        self._peripherals: dict[str, list[Peripheral]] = {
            "Pin": [self.led_1, self.led_2, self.button_1],
            "Uart": [self.uart_1],
            "I2C": [self.i2c_1],
        }

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

    def all_peripherals(self) -> list[Peripheral]:
        """Every emulated peripheral, across all object types."""
        return [
            peripheral for group in self._peripherals.values() for peripheral in group
        ]

    def _bind(self) -> None:
        """Claim the receive endpoint and bind it.

        No stale-file cleanup here: libzmq unlinks an ipc path before binding
        it, so a file left behind by a killed process is never a problem. The
        hazard is the reverse — that same unlink lets a newcomer displace a
        *live* emulator and take its endpoint, with no error on either side.

        Two guards, in order. The lock is the guarantee: it is atomic, so no
        other process using it can slip between the check and the bind. The
        probe is the fallback for an owner that holds no lock.
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
                except zmq.Again:
                    continue  # Receive timeout; recheck self.running.

                try:
                    json_message: dict[str, Any] = json.loads(message)
                except json.JSONDecodeError:
                    logger.warning("Received non-JSON message: %s", message)
                    continue

                try:
                    self._dispatch(json_message)
                except UnhandledMessageError as exc:
                    self._reject(json_message, exc)

        except Exception:
            logger.exception("Emulator thread error")
        finally:
            self.from_device_socket.close()
            # Released only once the socket is closed, so the endpoint is never
            # advertised as free while we still hold it.
            self._endpoint_lock.release()
            logger.debug("Emulator thread exiting")

    def _dispatch(self, json_message: dict[str, Any]) -> None:
        """Route one decoded message to the peripheral that owns it."""
        object_type = json_message.get("object")
        candidates = self._peripherals.get(str(object_type))
        if candidates is None:
            raise UnhandledMessageError(f"Unknown object type: {object_type}")

        for peripheral in candidates:
            if response := peripheral.handle_message(json_message):
                self.from_device_socket.send_string(response)
                return
        raise UnhandledMessageError(
            f"{object_type} not found: {json_message.get('name')}"
        )

    def _reject(self, json_message: dict[str, Any], exc: Exception) -> None:
        """Answer a message no peripheral owns, and keep serving.

        An unroutable message is a protocol error, not a fatal one: it says the
        device knows about a peripheral this emulator was not configured with,
        which is a bug in one registry or the other. Letting it propagate would
        kill the serve thread, and the device -- blocked in Transport::Receive
        on a PAIR socket -- would then hang until its timeout with no clue why.
        So log it and reply with Unhandled, which the C++ Transact() folds
        straight into the error channel as common::Error::kUnhandled.

        Only requests get a reply. A Response is the tail of an exchange the
        emulator itself started; answering one would leave an extra frame on
        the socket and desynchronize every exchange after it.
        """
        logger.error("Unhandled message: %s", exc)

        if json_message.get("type") != MessageType.Request:
            return

        # The union of the fields the three response structs deserialize, so
        # this decodes cleanly whichever one the device is expecting. Values
        # other than status are placeholders: Transact() rejects on a non-Ok
        # status before the caller ever sees them.
        self.from_device_socket.send_string(
            json.dumps(
                {
                    "type": MessageType.Response,
                    "object": json_message.get("object"),
                    "name": json_message.get("name"),
                    "state": PinState.Hi_Z,
                    "address": json_message.get("address", 0),
                    "data": [],
                    "bytes_transferred": 0,
                    "status": Status.Unhandled,
                }
            )
        )

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
        # PAIR blocks rather than drops, and SNDTIMEO bounds the wait, so a
        # sleep would buy nothing. Test-side readiness is _wait_for_process_ready.

    def stop(self) -> None:
        """Stop emulator and clean up resources."""
        logger.info("Stopping emulator")
        self.running = False

        self.emulator_thread.join(timeout=2.0)

        self.to_device_socket.close()
        self.context.term()
        logger.info("Emulator stopped")


def main() -> NoReturn:
    """Run the emulator until interrupted (Ctrl-C)."""
    logging.basicConfig(
        level=logging.INFO, format="[%(levelname)s] %(name)s: %(message)s"
    )
    emulator = DeviceEmulator()
    try:
        emulator.start()
        logger.info("Emulator running; press Ctrl-C to stop")
        while emulator.running:
            emulator.emulator_thread.join(0.5)
    except (KeyboardInterrupt, SystemExit):
        logger.info("Received keyboard interrupt")
        emulator.stop()
    sys.exit(0)


if __name__ == "__main__":
    main()
