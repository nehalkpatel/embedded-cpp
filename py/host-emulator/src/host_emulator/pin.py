"""Pin emulation for the host emulator."""

import json
from enum import Enum

from .common import Status


class Pin:
    """Emulates a digital pin (input/output)."""

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
