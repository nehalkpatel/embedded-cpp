"""Pin emulation for the host emulator."""

import json
import threading
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

    def wait_for_operation(self, operation, timeout=2.0):
        """Wait for a specific pin operation to occur.

        Args:
            operation: The operation to wait for ("Get" or "Set")
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
            if message.get("operation") == operation:
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

    def wait_for_state(self, state, timeout=2.0):
        """Wait for pin to reach a specific state.

        Args:
            state: The state to wait for (Pin.state.High, Pin.state.Low, etc.)
            timeout: Maximum time to wait in seconds

        Returns:
            True if state reached, False if timeout
        """
        # Check if already in desired state
        if self.state == state:
            return True

        event = threading.Event()

        def handler(message):
            # Call existing handler if present
            if old_handler is not None:
                old_handler(message)
            # Check our condition - look at the operation and resulting state
            if message.get("operation") == "Set" and message.get("state") == state.name:
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

    def wait_for_transitions(self, count, timeout=2.0):
        """Wait for a specific number of state transitions (toggles).

        Args:
            count: Number of transitions to wait for
            timeout: Maximum time to wait in seconds

        Returns:
            True if transitions occurred, False if timeout
        """
        transitions = [0]  # Use list to modify in closure
        event = threading.Event()
        last_state = [None]

        def handler(message):
            # Call existing handler if present
            if old_handler is not None:
                old_handler(message)
            # Check our condition
            current_state = message.get("state")
            if last_state[0] is not None and current_state != last_state[0]:
                transitions[0] += 1
                if transitions[0] >= count:
                    event.set()
            last_state[0] = current_state

        # Save old handler
        old_handler = self.on_request

        # Set chained handler
        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            # Restore old handler
            self.on_request = old_handler
