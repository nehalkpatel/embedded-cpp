"""The emulator must survive a message no peripheral owns.

``_dispatch`` raises ``UnhandledMessageError`` for an object type that is not
in the registry, and for a name that is. Both used to escape the serve loop,
be swallowed by its ``except Exception``, and take the emulator thread with
them -- leaving the device blocked in ``Transport::Receive`` on a PAIR socket
with nothing coming. The symptom was a hung test, never an error.

These are pure emulator tests -- no application binary -- so they run without
any of the --blinky/--uart-echo/--i2c-demo options.
"""

import json
from collections.abc import Generator
from pathlib import Path
from typing import Any

import pytest
import zmq

from host_emulator import DeviceEmulator
from host_emulator.common import MessageType, ObjectType, Operation, Status
from host_emulator.endpoint import endpoint_path
from host_emulator.pin import PinState

RECV_TIMEOUT_MS = 2000


@pytest.fixture
def standalone_emulator(tmp_path: Path) -> Generator[DeviceEmulator]:
    """A private emulator on its own endpoints, stopped afterwards."""
    from_device = f"ipc://{tmp_path}/from_device.ipc"
    to_device = f"ipc://{tmp_path}/to_device.ipc"
    device_emulator = DeviceEmulator(from_device, to_device)
    device_emulator.start()
    try:
        yield device_emulator
    finally:
        if device_emulator.running:
            device_emulator.stop()
        for endpoint in (from_device, to_device):
            path = endpoint_path(endpoint)
            if path is not None:
                path.unlink(missing_ok=True)
                path.with_name(path.name + ".lock").unlink(missing_ok=True)


@pytest.fixture
def device(standalone_emulator: DeviceEmulator) -> Generator[zmq.Socket[bytes]]:
    """A PAIR socket standing in for the C++ device side."""
    context = zmq.Context.instance()
    socket: zmq.Socket[bytes] = context.socket(zmq.PAIR)
    socket.setsockopt(zmq.RCVTIMEO, RECV_TIMEOUT_MS)
    socket.setsockopt(zmq.LINGER, 0)
    socket.connect(standalone_emulator.from_device_endpoint)
    try:
        yield socket
    finally:
        socket.close()


def _request(**overrides: Any) -> str:
    """A well-formed pin request, with fields overridden per test."""
    request: dict[str, Any] = {
        "type": MessageType.Request,
        "object": ObjectType.Pin,
        "name": "LED 1",
        "operation": Operation.Get,
        "state": PinState.Low,
    }
    request.update(overrides)
    return json.dumps(request)


@pytest.mark.parametrize(
    ("overrides", "case"),
    [
        ({"object": "Spi"}, "unregistered object type"),
        ({"name": "no_such_pin"}, "unregistered name"),
    ],
)
def test_unroutable_request_is_answered_not_dropped(
    device: zmq.Socket[bytes], overrides: dict[str, Any], case: str
) -> None:
    """An unroutable request gets an Unhandled response instead of silence."""
    device.send_string(_request(**overrides))

    response = json.loads(device.recv())

    assert response["status"] == Status.Unhandled, case
    assert response["type"] == MessageType.Response


def test_emulator_still_serves_after_an_unroutable_request(
    standalone_emulator: DeviceEmulator, device: zmq.Socket[bytes]
) -> None:
    """The serve thread survives, and the next real request still works."""
    device.send_string(_request(object="Spi"))
    device.recv()

    assert standalone_emulator.running

    device.send_string(_request(name="LED 1", operation=Operation.Set))
    response = json.loads(device.recv())

    assert response["status"] == Status.Ok


def test_unroutable_response_is_not_answered(
    standalone_emulator: DeviceEmulator, device: zmq.Socket[bytes]
) -> None:
    """A Response gets no reply -- one would desynchronize the socket.

    Answering the tail of an exchange would leave an extra frame queued, and
    every later request would read the previous request's reply.
    """
    device.send_string(_request(type=MessageType.Response, object="Spi"))

    with pytest.raises(zmq.Again):
        device.recv()

    assert standalone_emulator.running

    device.send_string(_request(name="LED 1", operation=Operation.Set))
    assert json.loads(device.recv())["status"] == Status.Ok
