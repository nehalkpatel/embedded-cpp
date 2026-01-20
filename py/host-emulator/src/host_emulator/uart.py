"""UART emulation for the host emulator."""

from __future__ import annotations

import json
import logging
import threading
from typing import TYPE_CHECKING, Any

from .common import Status

if TYPE_CHECKING:
    from collections.abc import Callable

    import zmq

logger = logging.getLogger(__name__)


class Uart:
    """Emulates a UART peripheral."""

    def __init__(self, name: str, to_device_socket: zmq.Socket[bytes]) -> None:
        self.name = name
        self.to_device_socket = to_device_socket
        self.rx_buffer = bytearray()  # Data waiting to be read
        self.on_response: Callable[[dict[str, Any]], None] | None = None
        self.on_request: Callable[[dict[str, Any]], None] | None = None

    def handle_request(self, message: dict[str, Any]) -> str:
        response: dict[str, Any] = {
            "type": "Response",
            "object": "Uart",
            "name": self.name,
            "data": [],
            "bytes_transferred": 0,
            "status": Status.InvalidOperation.name,
        }

        if message["operation"] == "Init":
            logger.info("[UART %s] Initialized", self.name)
            response.update({"status": Status.Ok.name})

        elif message["operation"] == "Send":
            data: list[int] = message.get("data", [])
            self.rx_buffer.extend(data)
            response.update(
                {
                    "bytes_transferred": len(data),
                    "status": Status.Ok.name,
                }
            )
            logger.info(
                "[UART %s] Received %d bytes: %s", self.name, len(data), bytes(data)
            )

        elif message["operation"] == "Receive":
            size: int = message.get("size", 0)
            bytes_to_send = min(size, len(self.rx_buffer))
            data = list(self.rx_buffer[:bytes_to_send])
            self.rx_buffer = self.rx_buffer[bytes_to_send:]
            response.update(
                {
                    "data": data,
                    "bytes_transferred": bytes_to_send,
                    "status": Status.Ok.name,
                }
            )
            logger.info(
                "[UART %s] Sent %d bytes: %s", self.name, bytes_to_send, bytes(data)
            )

        if self.on_request:
            self.on_request(message)
        return json.dumps(response)

    def send_data(self, data: bytes | list[int]) -> dict[str, Any]:
        """Send data to the device (emulator -> device)."""
        data_list = list(data) if isinstance(data, bytes) else data
        request = {
            "type": "Request",
            "object": "Uart",
            "name": self.name,
            "operation": "Receive",
            "data": data_list,
            "size": len(data_list),
            "timeout_ms": 0,
        }
        logger.debug("[UART %s] Sending data to device: %s", self.name, data)
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        logger.debug("[UART %s] Received response: %s", self.name, reply)
        result: dict[str, Any] = json.loads(reply)
        return result

    def handle_response(self, message: dict[str, Any]) -> None:
        logger.debug("[UART %s] Received response: %s", self.name, message)
        if self.on_response:
            self.on_response(message)

    def set_on_request(
        self, on_request: Callable[[dict[str, Any]], None] | None
    ) -> None:
        self.on_request = on_request

    def set_on_response(
        self, on_response: Callable[[dict[str, Any]], None] | None
    ) -> None:
        self.on_response = on_response

    def handle_message(self, message: dict[str, Any]) -> str | None:
        if message["object"] != "Uart":
            return None
        if message["name"] != self.name:
            return None
        if message["type"] == "Request":
            return self.handle_request(message)
        if message["type"] == "Response":
            self.handle_response(message)
            return None
        return None

    def wait_for_data(self, min_bytes: int = 1, timeout: float = 2.0) -> bool:
        """Wait for UART to receive at least min_bytes of data from device.

        Args:
            min_bytes: Minimum number of bytes to wait for
            timeout: Maximum time to wait in seconds

        Returns:
            True if data received, False if timeout
        """
        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            if message.get("operation") == "Send" and len(self.rx_buffer) >= min_bytes:
                event.set()

        # Check if we already have enough data
        if len(self.rx_buffer) >= min_bytes:
            return True

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler

    def wait_for_operation(self, operation: str, timeout: float = 2.0) -> bool:
        """Wait for a specific UART operation to occur.

        Args:
            operation: The operation to wait for ("Init", "Send", "Receive")
            timeout: Maximum time to wait in seconds

        Returns:
            True if operation occurred, False if timeout
        """
        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            if message.get("operation") == operation:
                event.set()

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler
