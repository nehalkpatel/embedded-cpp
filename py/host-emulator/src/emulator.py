#!/usr/bin/env python

import json
import sys
import zmq

from enum import Enum
from threading import Thread


class UnhandledMessageException(Exception):
    pass


class Pin:
    direction = Enum("direction", ["IN", "OUT"])
    state = Enum("state", ["Low", "High", "Hi_Z"])
    status = Enum(
        "status",
        [
            "Ok",
            "Unknown",
            "InvalidArgument",
            "InvalidState",
            "InvalidOperation",
        ],
    )

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
            "status": Pin.status.InvalidOperation.name,
        }
        if message["operation"] == "Get":
            response.update(
                {
                    "status": Pin.status.Ok.name,
                }
            )
        elif message["operation"] == "Set":
            self.state = Pin.state[message["state"]]
            response.update(
                {
                    "state": self.state.name,
                    "status": Pin.status.Ok.name,
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
        print(
            f"[Pin Handler] Setting on_request for {self.name}: {on_request}"
        )
        self.on_request = on_request

    def set_on_response(self, on_response):
        print(
            f"[Pin Handler] Setting on_response for {self.name}: {on_response}"
        )
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

    def UserLed1(self):
        return self.led_1

    def UserLed2(self):
        return self.led_2

    def UserButton1(self):
        return self.button_1

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
                                print(
                                    f"[Emulator] Sending response: {response}"
                                )
                                from_device_socket.send_string(response)
                                print("")
                                break
                        else:
                            raise UnhandledMessageException(
                                message, " - Pin not found"
                            )
                    else:
                        raise UnhandledMessageException(message, " - not Pin")
                else:
                    raise UnhandledMessageException(message, " - not JSON")
        finally:
            from_device_socket.close()
            from_device_context.term()

    def start(self):
        self.emulator_thread.start()

    def stop(self):
        self.running = False
        self.emulator_thread.join()


def main():
    emulator = DeviceEmulator()
    try:
        emulator.start()
        print("Sending Hello")
        emulator.to_device_socket.send_string("Hello")
        print("Waiting for reply")
        reply = emulator.to_device_socket.recv()
        print(f"Received reply: {reply}")
        reply = emulator.UserButton1().get_state()
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
