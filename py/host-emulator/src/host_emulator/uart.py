"""UART emulation for the host emulator."""

import json
import logging
from typing import Any

import zmq

from .common import MessageType, ObjectType, Operation, Status
from .peripheral import Peripheral

logger = logging.getLogger(__name__)


class Uart(Peripheral):
    """Emulates a UART peripheral."""

    OBJECT_TYPE = ObjectType.Uart

    def __init__(self, name: str, to_device_socket: zmq.Socket[bytes]) -> None:
        super().__init__(name, to_device_socket)
        self.rx_buffer = bytearray()  # Data the device has sent us

    def handle_request(self, message: dict[str, Any]) -> str:
        response: dict[str, Any] = {
            "type": MessageType.Response,
            "object": ObjectType.Uart,
            "name": self.name,
            "data": [],
            "bytes_transferred": 0,
            "status": Status.InvalidOperation,
        }

        if message["operation"] == Operation.Send:
            data: list[int] = message.get("data", [])
            self.rx_buffer.extend(data)
            response.update({"bytes_transferred": len(data), "status": Status.Ok})
            logger.info(
                "[UART %s] Received %d bytes: %s", self.name, len(data), bytes(data)
            )

        elif message["operation"] == Operation.Receive:
            size: int = message.get("size", 0)
            bytes_to_send = min(size, len(self.rx_buffer))
            data = list(self.rx_buffer[:bytes_to_send])
            self.rx_buffer = self.rx_buffer[bytes_to_send:]
            response.update(
                {
                    "data": data,
                    "bytes_transferred": bytes_to_send,
                    "status": Status.Ok,
                }
            )
            logger.info(
                "[UART %s] Sent %d bytes: %s", self.name, bytes_to_send, bytes(data)
            )

        self._notify_request(message)
        return json.dumps(response)

    def send_data(self, data: bytes | list[int]) -> dict[str, Any]:
        """Send data to the device (emulator -> device)."""
        data_list = list(data)
        return self._transact(
            {
                "type": MessageType.Request,
                "object": ObjectType.Uart,
                "name": self.name,
                "operation": Operation.Receive,
                "data": data_list,
                "size": len(data_list),
                "timeout_ms": 0,
            }
        )

    def wait_for_data(self, min_bytes: int = 1, timeout: float = 2.0) -> bool:
        """Wait until the device has sent at least ``min_bytes`` in total."""
        if len(self.rx_buffer) >= min_bytes:
            return True
        return self._wait_for(
            lambda message: message.get("operation") == Operation.Send
            and len(self.rx_buffer) >= min_bytes,
            timeout,
        )

    def wait_for_operation(self, operation: str, timeout: float = 2.0) -> bool:
        """Wait for a specific UART operation ("Send" or "Receive") to occur."""
        return self._wait_for(
            lambda message: message.get("operation") == operation, timeout
        )
