"""I2C emulation for the host emulator."""

import json

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
