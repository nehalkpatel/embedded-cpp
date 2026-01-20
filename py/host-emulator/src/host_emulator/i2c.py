"""I2C emulation for the host emulator."""

from __future__ import annotations

import json
import logging
import threading
from typing import TYPE_CHECKING, Any

from .common import Status

if TYPE_CHECKING:
    from collections.abc import Callable

logger = logging.getLogger(__name__)


class I2C:
    """Emulates an I2C controller/peripheral."""

    def __init__(self, name: str) -> None:
        self.name = name
        # Store data for each I2C address (address -> bytearray)
        self.device_buffers: dict[int, bytearray] = {}
        self.on_response: Callable[[dict[str, Any]], None] | None = None
        self.on_request: Callable[[dict[str, Any]], None] | None = None

    def handle_request(self, message: dict[str, Any]) -> str:
        response: dict[str, Any] = {
            "type": "Response",
            "object": "I2C",
            "name": self.name,
            "address": message.get("address", 0),
            "data": [],
            "bytes_transferred": 0,
            "status": Status.InvalidOperation.name,
        }

        address: int = message.get("address", 0)

        if message["operation"] == "Send":
            # Device is sending data to I2C peripheral
            data: list[int] = message.get("data", [])
            self.device_buffers[address] = bytearray(data)
            response.update(
                {
                    "bytes_transferred": len(data),
                    "status": Status.Ok.name,
                }
            )
            logger.info(
                "[I2C %s] Wrote %d bytes to address 0x%02X: %s",
                self.name,
                len(data),
                address,
                bytes(data),
            )

        elif message["operation"] == "Receive":
            # Device is receiving data from I2C peripheral
            size: int = message.get("size", 0)
            if address in self.device_buffers:
                bytes_to_send = min(size, len(self.device_buffers[address]))
                data = list(self.device_buffers[address][:bytes_to_send])
            else:
                bytes_to_send = 0
                data = []
            response.update(
                {
                    "data": data,
                    "bytes_transferred": bytes_to_send,
                    "status": Status.Ok.name,
                }
            )
            logger.info(
                "[I2C %s] Read %d bytes from address 0x%02X: %s",
                self.name,
                bytes_to_send,
                address,
                bytes(data),
            )

        if self.on_request:
            self.on_request(message)
        return json.dumps(response)

    def handle_response(self, message: dict[str, Any]) -> None:
        logger.debug("[I2C %s] Received response: %s", self.name, message)
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
        if message["object"] != "I2C":
            return None
        if message["name"] != self.name:
            return None
        if message["type"] == "Request":
            return self.handle_request(message)
        if message["type"] == "Response":
            self.handle_response(message)
            return None
        return None

    def write_to_device(self, address: int, data: bytes | list[int]) -> None:
        """Write data to a simulated I2C device (for testing)."""
        self.device_buffers[address] = bytearray(data)
        logger.debug(
            "[I2C %s] Device buffer at 0x%02X set to: %s",
            self.name,
            address,
            bytes(data),
        )

    def read_from_device(self, address: int) -> bytes:
        """Read data from a simulated I2C device (for testing)."""
        if address in self.device_buffers:
            return bytes(self.device_buffers[address])
        return b""

    def wait_for_operation(
        self, operation: str, address: int | None = None, timeout: float = 2.0
    ) -> bool:
        """Wait for a specific I2C operation to occur.

        Args:
            operation: The operation to wait for ("Send" or "Receive")
            address: Optional address to filter on (waits for any address if None)
            timeout: Maximum time to wait in seconds

        Returns:
            True if operation occurred, False if timeout
        """
        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            if old_handler is not None:
                old_handler(message)
            op_matches = message.get("operation") == operation
            addr_matches = address is None or message.get("address") == address
            if op_matches and addr_matches:
                event.set()

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler

    def wait_for_transactions(
        self, count: int, address: int | None = None, timeout: float = 2.0
    ) -> bool:
        """Wait for a specific number of I2C transactions (send or receive).

        Args:
            count: Number of transactions to wait for
            address: Optional address to filter on (waits for any address if None)
            timeout: Maximum time to wait in seconds

        Returns:
            True if transactions occurred, False if timeout
        """
        transactions = 0
        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            nonlocal transactions
            if old_handler is not None:
                old_handler(message)
            operation = message.get("operation")
            addr_matches = address is None or message.get("address") == address
            if operation in ("Send", "Receive") and addr_matches:
                transactions += 1
                if transactions >= count:
                    event.set()

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler
