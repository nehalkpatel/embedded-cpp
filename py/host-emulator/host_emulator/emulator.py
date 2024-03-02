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

    def __init__(self, name, direction, state):
        self.name = name
        self.direction = direction
        self.state = state

    def handle_request(self, message):
        if message["operation"] == "Get":
            return json.dumps(
                {
                    "type": "Response",
                    "object": "Pin",
                    "name": message["name"],
                    "state": self.state.name,
                    "status": Pin.status.Ok.name,
                }
            )
        elif message["operation"] == "Set":
            self.state = Pin.state[message["state"]]
            return json.dumps(
                {
                    "type": "Response",
                    "object": "Pin",
                    "name": self.name,
                    "state": self.state.name,
                    "status": Pin.status.Ok.name,
                }
            )
        else:
            return json.dumps(
                {
                    "type": "Response",
                    "object": "Pin",
                    "name": self.name,
                    "state": self.state.name,
                    "status": Pin.status.InvalidOperation.name,
                }
            )

    def handle_response(self, message):
        print(f"Received response: {message}")
        return None

    def handle_message(self, message):
        if message["type"] == "Request":
            return self.handle_request(message)
        elif message["type"] == "Response":
            return self.handle_response(message)


class DeviceEmulator:
    def __init__(self):
        print("Creating DeviceEmulator")
        self.emulator_thread = Thread(target=self.run)
        self.led_1 = Pin("LED 1", Pin.direction.OUT, Pin.state.Low)
        self.button_1 = Pin("Button 1", Pin.direction.IN, Pin.state.Low)
        self.pins = [self.led_1, self.button_1]
        self.running = False
        self.to_device_context = zmq.Context()
        self.to_device_socket = self.to_device_context.socket(zmq.PAIR)
        self.to_device_socket.connect("ipc:///tmp/emulator_device.ipc")

    def run(self):
        print("Starting emulator thread")
        try:
            self.from_device_context = zmq.Context()
            self.from_device_socket = self.from_device_context.socket(zmq.PAIR)
            self.from_device_socket.bind("ipc:///tmp/device_emulator.ipc")
            self.running = True
            while self.running:
                print("Waiting for message...")
                message = self.from_device_socket.recv()
                print(f"Received request: {message}")
                if message.startswith(b"{") and message.endswith(b"}"):
                    # JSON message
                    json_message = json.loads(message)
                    if json_message["object"] == "Pin":
                        for pin in self.pins:
                            response = pin.handle_message(json_message)
                            if response:
                                print(f"Sending response: {response}")
                                self.from_device_socket.send_string(response)
                                print("")
                                break
                            raise UnhandledMessageException(message)
                else:
                    raise UnhandledMessageException(message)
        finally:
            self.from_device_socket.close()
            self.from_device_context.term()

    def stop(self):
        self.running = False
        self.emulator_thread.join()


def main():
    emulator = DeviceEmulator()
    try:
        emulator.emulator_thread.start()
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
