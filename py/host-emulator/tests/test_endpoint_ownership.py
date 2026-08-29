"""Endpoint ownership tests for the emulator.

These cover the Python half of the same defect fixed in
``src/libs/mcu/host/zmq_transport.cpp``: libzmq unlinks an ipc path before
binding it, so a second emulator used to displace a running one silently. The
emulator additionally unlinked the path itself, unconditionally, which made it
certain rather than merely possible.

These are pure emulator tests -- no application binary -- so they run without
any of the --blinky/--uart-echo/--i2c-demo options.
"""

import socket
import subprocess
import sys
from collections.abc import Generator
from pathlib import Path

import pytest

from host_emulator import DeviceEmulator
from host_emulator.endpoint import EndpointLock, endpoint_path, has_live_owner


@pytest.fixture
def endpoints(tmp_path: Path) -> Generator[tuple[str, str]]:
    """A unique endpoint pair, with its lock files cleaned up afterwards."""
    from_device = f"ipc://{tmp_path}/from_device.ipc"
    to_device = f"ipc://{tmp_path}/to_device.ipc"
    yield from_device, to_device
    for endpoint in (from_device, to_device):
        path = endpoint_path(endpoint)
        if path is not None:
            path.unlink(missing_ok=True)
            path.with_name(path.name + ".lock").unlink(missing_ok=True)


def test_second_emulator_refuses_to_steal_endpoint(
    endpoints: tuple[str, str], tmp_path: Path
) -> None:
    """A second emulator must fail rather than displace a running one."""
    from_device, to_device = endpoints
    first = DeviceEmulator(from_device, to_device)
    first.start()

    try:
        second = DeviceEmulator(from_device, f"ipc://{tmp_path}/to_device2.ipc")
        try:
            with pytest.raises(RuntimeError, match="another live process"):
                second.start()
        finally:
            # Required, not tidiness: a DeviceEmulator opens its sockets in
            # __init__, so one whose start() failed still holds them, and
            # zmq_ctx_term blocks on an open socket. Skipping this hangs the
            # interpreter at exit rather than failing the test.
            second.stop()

        # The point of the test: the first emulator still owns the endpoint.
        # Before the fix the second bound successfully and every subsequent
        # connect() reached it instead, with no error on either side.
        probe = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            probe.connect(str(endpoint_path(from_device)))
        finally:
            probe.close()
    finally:
        first.stop()


def test_stale_socket_file_does_not_block_startup(
    endpoints: tuple[str, str],
) -> None:
    """A path left by a killed process must not stop the next run.

    The liveness probe has to distinguish "someone is listening" from "a file
    exists"; reading the latter as ownership would make any crash require
    manual cleanup.
    """
    from_device, to_device = endpoints
    path = endpoint_path(from_device)
    assert path is not None

    # Exactly what a SIGKILLed process leaves: bound, then closed without
    # unlinking.
    stale = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    stale.bind(str(path))
    stale.close()
    assert path.exists()

    emulator = DeviceEmulator(from_device, to_device)
    emulator.start()
    try:
        assert emulator.running
    finally:
        emulator.stop()


def test_lock_is_released_when_holder_dies(endpoints: tuple[str, str]) -> None:
    """A killed holder must not leave the endpoint locked.

    This is why the lock is flock and not an O_EXCL lock file: the kernel drops
    it when the fd closes, including on process death, so a crashed run needs no
    cleanup. An O_EXCL marker would need its own liveness check to distinguish
    held from abandoned -- the very problem the lock exists to solve.
    """
    from_device, _ = endpoints

    # A real subprocess rather than multiprocessing: the default start method
    # is forkserver, which pickles the target, and a locally-defined function
    # cannot be pickled.
    holder = subprocess.Popen(
        [
            sys.executable,
            "-c",
            "import sys, time;"
            "sys.path.insert(0, sys.argv[1]);"
            "from host_emulator.endpoint import EndpointLock;"
            "lock = EndpointLock();"
            "print(lock.try_acquire(sys.argv[2]), flush=True);"
            "time.sleep(60)",
            str(Path(__file__).parent.parent / "src"),
            from_device,
        ],
        stdout=subprocess.PIPE,
        text=True,
    )
    try:
        assert holder.stdout is not None
        assert holder.stdout.readline().strip() == "True"  # holder has the lock

        # Contended while alive.
        assert EndpointLock().try_acquire(from_device) is False
    finally:
        holder.kill()
        holder.wait(timeout=5)

    # Free again the moment the holder is gone.
    survivor = EndpointLock()
    assert survivor.try_acquire(from_device) is True
    survivor.release()


def test_has_live_owner_is_false_for_absent_endpoint(tmp_path: Path) -> None:
    """Nothing there at all is not ownership."""
    assert has_live_owner(f"ipc://{tmp_path}/never_created.ipc") is False


def test_non_ipc_endpoints_are_not_guarded() -> None:
    """tcp:// and inproc:// have no filesystem path, so both guards no-op."""
    assert endpoint_path("tcp://127.0.0.1:5555") is None
    assert has_live_owner("tcp://127.0.0.1:5555") is False
    assert EndpointLock().try_acquire("tcp://127.0.0.1:5555") is True
