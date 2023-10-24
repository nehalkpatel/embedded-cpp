#!/usr/bin/env python

import json
import zmq

from enum import Enum


class Pins:
    direction = Enum("direction", ["IN", "OUT"])
    state = Enum("state", ["Low", "High", "Hi_Z"])
    status = Enum(
        "status",
        ["Ok", "Unknown", "InvalidArgument", "InvalidState", "InvalidOperation"],
    )

    def __init__(self):
        self.pins = {}

    def add_pin(self, name, direction, state):
        self.pins[name] = {"direction": direction, "state": state}

    def handle_request(self, message):
        if message["operation"] == "get":
            return json.dumps(
                {
                    "type": "response",
                    "object": "pin",
                    "name": message["name"],
                    "state": self.pins[message["name"]]["state"].name,
                    "status": Pins.status.Ok.name,
                }
            )
        elif message["operation"] == "set":
            self.pins[message["name"]]["state"] = Pins.state[message["state"]]
            return json.dumps(
                {
                    "type": "response",
                    "object": "pin",
                    "name": message["name"],
                    "state": self.pins[message["name"]]["state"].name,
                    "status": Pins.status.Ok.name,
                }
            )
        else:
            return json.dumps(
                {
                    "type": "response",
                    "object": "pin",
                    "name": message["name"],
                    "state": self.pins[message["name"]]["state"].name,
                    "status": Pins.status.InvalidOperation.name,
                }
            )

    def handle_response(self, message):
        print(f"Received response: {message}")
        return None

    def handle_message(self, message):
        if message["type"] == "request":
            return self.handle_request(message)
        elif message["type"] == "response":
            return self.handle_response(message)


class DeviceEmulator:
    def __init__(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PAIR)
        self.socket.bind("ipc:///tmp/device_emulator.ipc")
        self.pins = Pins()
        self.pins.add_pin("LED 1", Pins.direction.OUT, Pins.state.Low)

    def run(self):
        while True:
            message = self.socket.recv()
            print(f"Received request: {message}")
            if message.startswith(b"{") and message.endswith(b"}"):
                # JSON message
                json_message = json.loads(message)
                if json_message["object"] == "pin":
                    response = self.pins.handle_message(json_message)
                    if response:
                        print(f"Sending response: {response}")
                        self.socket.send_string(response)
                        print("")


def main():
    emulator = DeviceEmulator()
    emulator.run()


if __name__ == "__main__":
    main()
