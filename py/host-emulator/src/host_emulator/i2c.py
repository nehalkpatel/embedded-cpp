"""I2C emulation for the host emulator."""

import json
import logging
from typing import Any

from .common import MessageType, ObjectType, Operation, Status
from .peripheral import Peripheral

logger = logging.getLogger(__name__)


class I2C(Peripheral):
    """Emulates an I2C bus with one buffer per device address.

    Purely reactive: it never initiates traffic, so it takes no device socket.
    """

    OBJECT_TYPE = ObjectType.I2C

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Store data for each I2C address (address -> bytearray)
        self.device_buffers: dict[int, bytearray] = {}

    def handle_request(self, message: dict[str, Any]) -> str:
        address: int = message.get("address", 0)
        response: dict[str, Any] = {
            "type": MessageType.Response,
            "object": ObjectType.I2C,
            "name": self.name,
            "address": address,
            "data": [],
            "bytes_transferred": 0,
            "status": Status.InvalidOperation,
        }

        if message["operation"] == Operation.Send:
            # Device is sending data to I2C peripheral
            data: list[int] = message.get("data", [])
            self.device_buffers[address] = bytearray(data)
            response.update({"bytes_transferred": len(data), "status": Status.Ok})
            logger.info(
                "[I2C %s] Wrote %d bytes to address 0x%02X: %s",
                self.name,
                len(data),
                address,
                bytes(data),
            )

        elif message["operation"] == Operation.Receive:
            # Device is receiving data from I2C peripheral
            size: int = message.get("size", 0)
            buffer = self.device_buffers.get(address, bytearray())
            bytes_to_send = min(size, len(buffer))
            data = list(buffer[:bytes_to_send])
            response.update(
                {
                    "data": data,
                    "bytes_transferred": bytes_to_send,
                    "status": Status.Ok,
                }
            )
            logger.info(
                "[I2C %s] Read %d bytes from address 0x%02X: %s",
                self.name,
                bytes_to_send,
                address,
                bytes(data),
            )

        self._notify_request(message)
        return json.dumps(response)

    def write_to_device(self, address: int, data: bytes | list[int]) -> None:
        """Write data to a simulated I2C device (for testing)."""
        self.device_buffers[address] = bytearray(data)
        logger.debug(
            "[I2C %s] Device buffer at 0x%02X set to: %s",
            self.name,
            address,
            bytes(data),
        )

    def wait_for_operation(
        self, operation: str, address: int | None = None, timeout: float = 2.0
    ) -> bool:
        """Wait for an I2C operation, optionally filtered to one address."""
        return self._wait_for(
            lambda message: message.get("operation") == operation
            and (address is None or message.get("address") == address),
            timeout,
        )

    def wait_for_transactions(
        self, count: int, address: int | None = None, timeout: float = 2.0
    ) -> bool:
        """Wait for ``count`` transactions (Send or Receive), optionally by address."""
        transactions = 0

        def is_final_transaction(message: dict[str, Any]) -> bool:
            nonlocal transactions
            operation = message.get("operation")
            addr_matches = address is None or message.get("address") == address
            if operation in (Operation.Send, Operation.Receive) and addr_matches:
                transactions += 1
            return transactions >= count

        return self._wait_for(is_final_transaction, timeout)
