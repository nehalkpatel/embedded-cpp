"""UART emulation for the host emulator."""

import json
import threading

from .common import Status


class Uart:
    """Emulates a UART peripheral."""

    def __init__(self, name, to_device_socket):
        self.name = name
        self.to_device_socket = to_device_socket
        self.rx_buffer = bytearray()  # Data waiting to be read
        self.on_response = None
        self.on_request = None

    def handle_request(self, message):
        response = {
            "type": "Response",
            "object": "Uart",
            "name": self.name,
            "data": [],
            "bytes_transferred": 0,
            "status": Status.InvalidOperation.name,
        }

        if message["operation"] == "Init":
            # Initialize UART with given configuration
            print(f"[UART {self.name}] Initialized")
            response.update({"status": Status.Ok.name})

        elif message["operation"] == "Send":
            # Receive data from the device and store in RX buffer
            data = message.get("data", [])
            self.rx_buffer.extend(data)
            response.update(
                {
                    "bytes_transferred": len(data),
                    "status": Status.Ok.name,
                }
            )
            print(f"[UART {self.name}] Received {len(data)} bytes: {bytes(data)}")

        elif message["operation"] == "Receive":
            # Send buffered data back to the device
            size = message.get("size", 0)
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
            print(f"[UART {self.name}] Sent {bytes_to_send} bytes: {bytes(data)}")

        if self.on_request:
            self.on_request(message)
        return json.dumps(response)

    def send_data(self, data):
        """Send data to the device (emulator -> device)"""
        request = {
            "type": "Request",
            "object": "Uart",
            "name": self.name,
            "operation": "Receive",
            "data": list(data),
            "size": len(data),
            "timeout_ms": 0,
        }
        print(f"[UART {self.name}] Sending data to device: {data}")
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        print(f"[UART {self.name}] Received response: {reply}")
        return json.loads(reply)

    def handle_response(self, message):
        print(f"[UART {self.name}] Received response: {message}")
        if self.on_response:
            self.on_response(message)
        return None

    def set_on_request(self, on_request):
        self.on_request = on_request

    def set_on_response(self, on_response):
        self.on_response = on_response

    def handle_message(self, message):
        if message["object"] != "Uart":
            return None
        if message["name"] != self.name:
            return None
        if message["type"] == "Request":
            return self.handle_request(message)
        if message["type"] == "Response":
            return self.handle_response(message)

    def wait_for_data(self, min_bytes=1, timeout=2.0):
        """Wait for UART to receive at least min_bytes of data from device.

        Args:
            min_bytes: Minimum number of bytes to wait for
            timeout: Maximum time to wait in seconds

        Returns:
            True if data received, False if timeout
        """
        event = threading.Event()

        def handler(message):
            if message.get("operation") == "Send" and len(self.rx_buffer) >= min_bytes:
                event.set()

        # Save old handler
        old_handler = self.on_request

        # Set temporary handler
        self.on_request = handler

        # Check if we already have enough data
        if len(self.rx_buffer) >= min_bytes:
            self.on_request = old_handler
            return True

        try:
            return event.wait(timeout)
        finally:
            # Restore old handler
            self.on_request = old_handler

    def wait_for_operation(self, operation, timeout=2.0):
        """Wait for a specific UART operation to occur.

        Args:
            operation: The operation to wait for ("Init", "Send", "Receive")
            timeout: Maximum time to wait in seconds

        Returns:
            True if operation occurred, False if timeout
        """
        event = threading.Event()

        def handler(message):
            if message.get("operation") == operation:
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
