"""Shared behavior for emulated peripherals.

Each peripheral (Pin, Uart, I2C) supplies only its protocol —
``handle_request`` — and inherits everything else: message routing,
request/response hooks, the blocking request helper, and the wait-for-condition
machinery the tests build on.
"""

import json
import logging
import threading
from abc import ABC, abstractmethod
from collections.abc import Callable
from typing import Any, ClassVar

import zmq

from .common import MessageType

logger = logging.getLogger(__name__)


class Peripheral(ABC):
    """An emulated peripheral addressed by object type and name."""

    #: The wire value of the ``object`` field this peripheral answers to.
    OBJECT_TYPE: ClassVar[str]

    def __init__(
        self, name: str, to_device_socket: zmq.Socket[bytes] | None = None
    ) -> None:
        self.name = name
        self.to_device_socket = to_device_socket
        self.on_request: Callable[[dict[str, Any]], None] | None = None
        self.on_response: Callable[[dict[str, Any]], None] | None = None

    @abstractmethod
    def handle_request(self, message: dict[str, Any]) -> str:
        """Answer one request from the device; returns the encoded response."""

    def handle_message(self, message: dict[str, Any]) -> str | None:
        """Route a decoded message: the response to send, or None if not ours."""
        if message["object"] != self.OBJECT_TYPE or message["name"] != self.name:
            return None
        if message["type"] == MessageType.Request:
            return self.handle_request(message)
        if message["type"] == MessageType.Response:
            self.handle_response(message)
        return None

    def handle_response(self, message: dict[str, Any]) -> None:
        logger.debug(
            "[%s %s] Received response: %s", self.OBJECT_TYPE, self.name, message
        )
        if self.on_response:
            self.on_response(message)

    def set_on_request(
        self, on_request: Callable[[dict[str, Any]], None] | None
    ) -> None:
        self.on_request = on_request

    def _notify_request(self, message: dict[str, Any]) -> None:
        """Invoke the request hook; handle_request implementations call this."""
        if self.on_request:
            self.on_request(message)

    def _transact(self, request: dict[str, Any]) -> dict[str, Any]:
        """Send one request to the device and return the decoded reply."""
        if self.to_device_socket is None:
            msg = f"{self.OBJECT_TYPE} {self.name} has no device socket"
            raise RuntimeError(msg)
        logger.debug(
            "[%s %s] Sending request: %s", self.OBJECT_TYPE, self.name, request
        )
        self.to_device_socket.send_string(json.dumps(request))
        reply = self.to_device_socket.recv()
        response: dict[str, Any] = json.loads(reply)
        self.handle_response(response)
        return response

    def _wait_for(
        self, predicate: Callable[[dict[str, Any]], bool], timeout: float
    ) -> bool:
        """Block until a request satisfying ``predicate`` arrives, or time out.

        Installs a temporary request hook, chained after any existing one so
        nested waits observe every message, and always restores the original.
        """
        event = threading.Event()
        old_handler = self.on_request

        def handler(message: dict[str, Any]) -> None:
            if old_handler is not None:
                old_handler(message)
            if predicate(message):
                event.set()

        self.on_request = handler
        try:
            return event.wait(timeout)
        finally:
            self.on_request = old_handler
