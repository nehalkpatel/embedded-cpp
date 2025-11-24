#!/usr/bin/env python

import json
import sys
from enum import Enum
from threading import Thread

import zmq


class UnhandledMessageError(Exception):
    pass


class Status(Enum):
    Ok = "Ok"
    Unknown = "Unknown"
    InvalidArgument = "InvalidArgument"
    InvalidState = "InvalidState"
    InvalidOperation = "InvalidOperation"


class Pin:
    direction = Enum("direction", ["IN", "OUT"])
    state = Enum("state", ["Low", "High", "Hi_Z"])

    def __init__(self, name, direction, state, to_device_socket):
        self.name = name
        self.direction = direction
        self.state = state
        self.to_device_socket = to_device_socket
        self.on_response = None
        self.on_request = None

    def handle_request(self, message):
        response = {
            "type": "Response",
            "object": "Pin",
            "name": self.name,
            "state": self.state.name,
            "status": Status.InvalidOperation.name,
        }
        if message["operation"] == "Get":
            response.update(
                {
                    "status": Status.Ok.name,
                }
            )
        elif message["operation"] == "Set":
            self.state = Pin.state[message["state"]]
            response.update(
                {
                    "state": self.state.name,
                    "status": Status.Ok.name,
                }
            )
        else:
            pass
            # default response status is InvalidOperation
        if self.on_request:
            self.on_request(message)
        return json.dumps(response)

    def set_state(self, state):
        self.state = state
        request = {
            "type": "Request",
            "object": "Pin",
            "name": self.name,
            "operation": "Set",
            "state": self.state.name,
        }
        print(f"[Pin Set] Sending request: {request}")
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        print(f"[Pin Set] Received response: {reply}")
        self.handle_response(json.loads(reply))
        return json.loads(reply)

    def get_state(self):
        request = {
            "type": "Request",
            "object": "Pin",
            "name": self.name,
            "operation": "Get",
            "state": self.state.Hi_Z.name,
        }
        print(f"[Pin Get] Sending request: {request}")
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        print(f"[Pin Get] Received response: {reply}")
        self.handle_response(json.loads(reply))
        return json.loads(reply)

    def handle_response(self, message):
        print(f"[Pin Handler] Received response: {message}")
        if self.on_response:
            self.on_response(message)
        return None

    def set_on_request(self, on_request):
        print(f"[Pin Handler] Setting on_request for {self.name}: {on_request}")
        self.on_request = on_request

    def set_on_response(self, on_response):
        print(f"[Pin Handler] Setting on_response for {self.name}: {on_response}")
        self.on_response = on_response

    def handle_message(self, message):
        if message["object"] != "Pin":
            return None
        if message["name"] != self.name:
            return None
        if message["type"] == "Request":
            return self.handle_request(message)
        if message["type"] == "Response":
            return self.handle_response(message)


class Uart:
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


class I2C:
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
            f"[I2C {self.name}] Device buffer at 0x{address:02X} set to: "
            f"{bytes(data)}"
        )

    def read_from_device(self, address):
        """Read data from a simulated I2C device (for testing)"""
        if address in self.device_buffers:
            return bytes(self.device_buffers[address])
        return b""


class DeviceEmulator:
    def __init__(self):
        print("Creating DeviceEmulator")
        self.emulator_thread = Thread(target=self.run)
        self.running = False
        self.to_device_context = zmq.Context()
        self.to_device_socket = self.to_device_context.socket(zmq.PAIR)
        self.to_device_socket.connect("ipc:///tmp/emulator_device.ipc")
        self.led_1 = Pin(
            "LED 1", Pin.direction.OUT, Pin.state.Low, self.to_device_socket
        )
        self.led_2 = Pin(
            "LED 2", Pin.direction.OUT, Pin.state.Low, self.to_device_socket
        )
        self.button_1 = Pin(
            "Button 1", Pin.direction.IN, Pin.state.Low, self.to_device_socket
        )
        self.pins = [self.led_1, self.led_2, self.button_1]

        self.uart_1 = Uart("UART 1", self.to_device_socket)
        self.uarts = [self.uart_1]

        self.i2c_1 = I2C("I2C 1")
        self.i2cs = [self.i2c_1]

    def user_led1(self):
        return self.led_1

    def user_led2(self):
        return self.led_2

    def user_button1(self):
        return self.button_1

    def uart1(self):
        return self.uart_1

    def i2c1(self):
        return self.i2c_1

    def run(self):
        print("Starting emulator thread")
        try:
            from_device_context = zmq.Context()
            from_device_socket = from_device_context.socket(zmq.PAIR)
            from_device_socket.bind("ipc:///tmp/device_emulator.ipc")
            self.running = True
            while self.running:
                print("Waiting for message...")
                message = from_device_socket.recv()
                print(f"[Emulator] Received request: {message}")
                if message.startswith(b"{") and message.endswith(b"}"):
                    # JSON message
                    json_message = json.loads(message)
                    if json_message["object"] == "Pin":
                        for pin in self.pins:
                            if response := pin.handle_message(json_message):
                                print(f"[Emulator] Sending response: {response}")
                                from_device_socket.send_string(response)
                                print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - Pin not found")
                    elif json_message["object"] == "Uart":
                        for uart in self.uarts:
                            if response := uart.handle_message(json_message):
                                print(f"[Emulator] Sending response: {response}")
                                from_device_socket.send_string(response)
                                print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - Uart not found")
                    elif json_message["object"] == "I2C":
                        for i2c in self.i2cs:
                            if response := i2c.handle_message(json_message):
                                print(f"[Emulator] Sending response: {response}")
                                from_device_socket.send_string(response)
                                print("")
                                break
                        else:
                            raise UnhandledMessageError(message, " - I2C not found")
                    else:
                        raise UnhandledMessageError(
                            message, f" - unknown object type: {json_message['object']}"
                        )
                else:
                    raise UnhandledMessageError(message, " - not JSON")
        finally:
            from_device_socket.close()
            from_device_context.term()

    def start(self):
        self.emulator_thread.start()

    def stop(self):
        self.running = False
        self.emulator_thread.join()

    def uart_initialized(self, name):
        """Check if a UART with the given name exists."""
        return any(uart.name == name for uart in self.uarts)

    def get_uart_tx_data(self, name):
        """Get data that was transmitted (sent) from the device to the emulator."""
        for uart in self.uarts:
            if uart.name == name:
                if len(uart.rx_buffer) > 0:
                    data = list(uart.rx_buffer)
                    return data
                return None
        return None

    def clear_uart_tx_data(self, name):
        """Clear the TX buffer (data received from device)."""
        for uart in self.uarts:
            if uart.name == name:
                uart.rx_buffer.clear()
                return True
        return False

    def uart_send_to_device(self, name, data):
        """Send data from emulator to device (simulating external UART input)."""
        for uart in self.uarts:
            if uart.name == name:
                return uart.send_data(data)
        return None

    def get_pin_state(self, name):
        """Get the current state of a pin."""
        for pin in self.pins:
            if pin.name == name:
                return pin.state
        return None


def main():
    emulator = DeviceEmulator()
    try:
        emulator.start()
        print("Sending Hello")
        emulator.to_device_socket.send_string("Hello")
        print("Waiting for reply")
        reply = emulator.to_device_socket.recv()
        print(f"Received reply: {reply}")
        reply = emulator.user_button1().get_state()
        print(f"Received reply: {reply}")
        while emulator.running:
            emulator.emulator_thread.join(0.5)
    except (KeyboardInterrupt, SystemExit):
        print("main Received keyboard interrupt")
        emulator.stop()
        emulator.to_device_socket.close()
        emulator.to_device_context.term()
        sys.exit(0)


if __name__ == "__main__":
    main()
