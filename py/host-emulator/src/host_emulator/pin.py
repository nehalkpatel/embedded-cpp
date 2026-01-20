"""Pin emulation for the host emulator."""

from __future__ import annotations

import json
import logging
import threading
from enum import Enum
from typing import TYPE_CHECKING, Any

from .common import Status

if TYPE_CHECKING:
    from collections.abc import Callable

    import zmq

logger = logging.getLogger(__name__)


class PinDirection(Enum):
    """Pin direction configuration."""

    IN = "IN"
    OUT = "OUT"


class PinState(Enum):
    """Pin state values."""

    Low = "Low"
    High = "High"
    Hi_Z = "Hi_Z"


class Pin:
    """Emulates a digital pin (input/output)."""

    def __init__(
        self,
        name: str,
        pin_direction: PinDirection,
        initial_state: PinState,
        to_device_socket: zmq.Socket[bytes],
    ) -> None:
        self.name = name
        self.pin_direction = pin_direction
        self.state = initial_state
        self.to_device_socket = to_device_socket
        self.on_response: Callable[[dict[str, Any]], None] | None = None
        self.on_request: Callable[[dict[str, Any]], None] | None = None

    def handle_request(self, message: dict[str, Any]) -> str:
        response: dict[str, Any] = {
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
            self.state = PinState[message["state"]]
            response.update(
                {
                    "state": self.state.name,
                    "status": Status.Ok.name,
                }
            )
        # default response status is InvalidOperation

        if self.on_request:
            self.on_request(message)
        return json.dumps(response)

    def set_state(self, state: PinState) -> dict[str, Any]:
        self.state = state
        request = {
            "type": "Request",
            "object": "Pin",
            "name": self.name,
            "operation": "Set",
            "state": self.state.name,
        }
        logger.debug("[Pin Set] Sending request: %s", request)
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        logger.debug("[Pin Set] Received response: %s", reply)
        response: dict[str, Any] = json.loads(reply)
        self.handle_response(response)
        return response

    def get_state(self) -> dict[str, Any]:
        request = {
            "type": "Request",
            "object": "Pin",
            "name": self.name,
            "operation": "Get",
            "state": PinState.Hi_Z.name,
        }
        logger.debug("[Pin Get] Sending request: %s", request)
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        logger.debug("[Pin Get] Received response: %s", reply)
        response: dict[str, Any] = json.loads(reply)
        self.handle_response(response)
        return response

    def handle_response(self, message: dict[str, Any]) -> None:
        logger.debug("[Pin Handler] Received response: %s", message)
        if self.on_response:
            self.on_response(message)

    def set_on_request(
        self, on_request: Callable[[dict[str, Any]], None] | None
    ) -> None:
        logger.debug(
            "[Pin Handler] Setting on_request for %s: %s", self.name, on_request
        )
        self.on_request = on_request

    def set_on_response(
        self, on_response: Callable[[dict[str, Any]], None] | None
    ) -> None:
        logger.debug(
            "[Pin Handler] Setting on_response for %s: %s", self.name, on_response
        )
        self.on_response = on_response

    def handle_message(self, message: dict[str, Any]) -> str | None:
        if message["object"] != "Pin":
            return None
        if message["name"] != self.name:
            return None
        if message["type"] == "Request":
            return self.handle_request(message)
        if message["type"] == "Response":
            self.handle_response(message)
            return None
        return None

    def wait_for_operation(self, operation: str, timeout: float = 2.0) -> bool:
        """Wait for a specific pin operation to occur.

        Args:
            operation: The operation to wait for ("Get" or "Set")
            timeout: Maximum time to wait in seconds

        Returns:
            True if operation occurred, False if timeout
        """
        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            if old_handler is not None:
                old_handler(message)
            if message.get("operation") == operation:
                event.set()

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler

    def wait_for_state(self, state: PinState, timeout: float = 2.0) -> bool:
        """Wait for pin to reach a specific state.

        Args:
            state: The state to wait for (PinState.High, PinState.Low, etc.)
            timeout: Maximum time to wait in seconds

        Returns:
            True if state reached, False if timeout
        """
        if self.state == state:
            return True

        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            if old_handler is not None:
                old_handler(message)
            if message.get("operation") == "Set" and message.get("state") == state.name:
                event.set()

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler

    def wait_for_transitions(self, count: int, timeout: float = 2.0) -> bool:
        """Wait for a specific number of state transitions (toggles).

        Args:
            count: Number of transitions to wait for
            timeout: Maximum time to wait in seconds

        Returns:
            True if transitions occurred, False if timeout
        """
        transitions = 0
        event = threading.Event()
        last_state: str | None = None
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            nonlocal transitions, last_state
            if old_handler is not None:
                old_handler(message)
            current_state = message.get("state")
            if last_state is not None and current_state != last_state:
                transitions += 1
                if transitions >= count:
                    event.set()
            last_state = current_state

        self.on_request = handler

        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler
