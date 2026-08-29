"""Pin emulation for the host emulator."""

import json
from enum import StrEnum
from typing import Any

import zmq

from .common import MessageType, ObjectType, Operation, Status
from .peripheral import Peripheral


class PinDirection(StrEnum):
    """Pin direction from the device's point of view.

    Wire values mirror C++ ``mcu::PinDirection`` (direction is not currently
    sent on the wire, but the vocabularies must not drift).
    """

    Input = "Input"
    Output = "Output"


class PinState(StrEnum):
    """Pin state values; wire values mirror C++ ``mcu::PinState``."""

    Low = "Low"
    High = "High"
    Hi_Z = "Hi_Z"


class Pin(Peripheral):
    """Emulates a digital pin (input/output)."""

    OBJECT_TYPE = ObjectType.Pin

    def __init__(
        self,
        name: str,
        direction: PinDirection,
        initial_state: PinState,
        to_device_socket: zmq.Socket[bytes],
    ) -> None:
        super().__init__(name, to_device_socket)
        self.direction = direction
        self.state = initial_state

    def handle_request(self, message: dict[str, Any]) -> str:
        response: dict[str, Any] = {
            "type": MessageType.Response,
            "object": ObjectType.Pin,
            "name": self.name,
            "state": self.state,
            "status": Status.InvalidOperation,
        }
        if message["operation"] == Operation.Get:
            response.update({"status": Status.Ok})
        elif (
            message["operation"] == Operation.Set
            and self.direction is PinDirection.Output
        ):
            self.state = PinState(message["state"])
            response.update({"state": self.state, "status": Status.Ok})
        # A device Set on its own input pin keeps the default InvalidOperation
        # status: the emulator, not the device, drives input pins.

        self._notify_request(message)
        return json.dumps(response)

    def set_state(self, state: PinState) -> dict[str, Any]:
        """Drive the pin from the emulator side (e.g. press a button)."""
        self.state = state
        return self._transact(
            {
                "type": MessageType.Request,
                "object": ObjectType.Pin,
                "name": self.name,
                "operation": Operation.Set,
                "state": self.state,
            }
        )

    def get_state(self) -> dict[str, Any]:
        """Ask the device for its view of the pin."""
        return self._transact(
            {
                "type": MessageType.Request,
                "object": ObjectType.Pin,
                "name": self.name,
                "operation": Operation.Get,
                "state": PinState.Hi_Z,
            }
        )

    def wait_for_operation(self, operation: str, timeout: float = 2.0) -> bool:
        """Wait for a specific pin operation ("Get" or "Set") to occur."""
        return self._wait_for(
            lambda message: message.get("operation") == operation, timeout
        )

    def wait_for_state(self, state: PinState, timeout: float = 2.0) -> bool:
        """Wait for the pin to reach a specific state."""
        if self.state == state:
            return True
        return self._wait_for(
            lambda message: message.get("operation") == Operation.Set
            and message.get("state") == state,
            timeout,
        )

    def wait_for_transitions(self, count: int, timeout: float = 2.0) -> bool:
        """Wait for a specific number of state transitions (toggles)."""
        transitions = 0
        last_state: str | None = None

        def is_final_transition(message: dict[str, Any]) -> bool:
            nonlocal transitions, last_state
            current_state = message.get("state")
            if last_state is not None and current_state != last_state:
                transitions += 1
            last_state = current_state
            return transitions >= count

        return self._wait_for(is_final_transition, timeout)
