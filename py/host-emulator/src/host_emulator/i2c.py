"""I2C emulation for the host emulator."""

import json
import threading

from .common import Status


class I2C:
    """Emulates an I2C controller/peripheral."""

    def __init__(self, name):
        self.name = name
        # Store data for each I2C address (address -> bytearray)
        self.device_buffers = {}
        self.on_response = None
        self.on_request = None

    def handle_request(self, message):
        response = {
            "type": "Response",
            "object": "I2C",
            "name": self.name,
            "address": message.get("address", 0),
            "data": [],
            "bytes_transferred": 0,
            "status": Status.InvalidOperation.name,
        }

        address = message.get("address", 0)

        if message["operation"] == "Send":
            # Device is sending data to I2C peripheral
            # Store the data in the buffer for this address
            data = message.get("data", [])
            if address not in self.device_buffers:
                self.device_buffers[address] = bytearray()
            self.device_buffers[address] = bytearray(data)
            response.update(
                {
                    "bytes_transferred": len(data),
                    "status": Status.Ok.name,
                }
            )
            print(
                f"[I2C {self.name}] Wrote {len(data)} bytes to address "
                f"0x{address:02X}: {bytes(data)}"
            )

        elif message["operation"] == "Receive":
            # Device is receiving data from I2C peripheral
            # Return data from the buffer for this address
            size = message.get("size", 0)
            if address in self.device_buffers:
                bytes_to_send = min(size, len(self.device_buffers[address]))
                data = list(self.device_buffers[address][:bytes_to_send])
            else:
                # No data available, return empty
                bytes_to_send = 0
                data = []
            response.update(
                {
                    "data": data,
                    "bytes_transferred": bytes_to_send,
                    "status": Status.Ok.name,
                }
            )
            print(
                f"[I2C {self.name}] Read {bytes_to_send} bytes from address "
                f"0x{address:02X}: {bytes(data)}"
            )

        if self.on_request:
            self.on_request(message)
        return json.dumps(response)

    def handle_response(self, message):
        print(f"[I2C {self.name}] Received response: {message}")
        if self.on_response:
            self.on_response(message)
        return None

    def set_on_request(self, on_request):
        self.on_request = on_request

    def set_on_response(self, on_response):
        self.on_response = on_response

    def handle_message(self, message):
        if message["object"] != "I2C":
            return None
        if message["name"] != self.name:
            return None
        if message["type"] == "Request":
            return self.handle_request(message)
        if message["type"] == "Response":
            return self.handle_response(message)

    def write_to_device(self, address, data):
        """Write data to a simulated I2C device (for testing)"""
        if address not in self.device_buffers:
            self.device_buffers[address] = bytearray()
        self.device_buffers[address] = bytearray(data)
        print(
            f"[I2C {self.name}] Device buffer at 0x{address:02X} set to: {bytes(data)}"
        )

    def read_from_device(self, address):
        """Read data from a simulated I2C device (for testing)"""
        if address in self.device_buffers:
            return bytes(self.device_buffers[address])
        return b""

    def wait_for_operation(self, operation, address=None, timeout=2.0):
        """Wait for a specific I2C operation to occur.

        Args:
            operation: The operation to wait for ("Send" or "Receive")
            address: Optional address to filter on (waits for any address if None)
            timeout: Maximum time to wait in seconds

        Returns:
            True if operation occurred, False if timeout
        """
        event = threading.Event()

        def handler(message):
            # Call existing handler if present
            if old_handler is not None:
                old_handler(message)
            # Check our condition
            if (
                message.get("operation") == operation
                and address is None
                or message.get("address") == address
            ):
                event.set()

        # Save old handler
        old_handler = self.on_request

        # Set chained handler
        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            # Restore old handler
            self.on_request = old_handler

    def wait_for_transactions(self, count, address=None, timeout=2.0):
        """Wait for a specific number of I2C transactions (send or receive).

        Args:
            count: Number of transactions to wait for
            address: Optional address to filter on (waits for any address if None)
            timeout: Maximum time to wait in seconds

        Returns:
            True if transactions occurred, False if timeout
        """
        transactions = [0]  # Use list to modify in closure
        event = threading.Event()

        def handler(message):
            # Call existing handler if present
            if old_handler is not None:
                old_handler(message)
            # Check our condition
            operation = message.get("operation")
            if operation in ("Send", "Receive") and (
                address is None or message.get("address") == address
            ):
                transactions[0] += 1
                if transactions[0] >= count:
                    event.set()

        # Save old handler
        old_handler = self.on_request

        # Set temporary handler
        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            # Restore old handler
            self.on_request = old_handler
