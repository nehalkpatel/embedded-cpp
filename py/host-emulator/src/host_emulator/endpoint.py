"""Ownership guards for ipc:// endpoints.

The Python counterpart of ``mcu::EndpointLock`` and
``ZmqTransport::EndpointHasLiveOwner`` in ``src/libs/mcu/host/zmq_transport.cpp``.

Both ends of the emulator IPC need this, for the same reason: libzmq unlinks an
ipc path before binding it, unconditionally, and will happily displace a live
listener and take its rendezvous name. Neither side sees an error -- the
original owner keeps its existing connections, because the inode outlives the
name, but every later connect() reaches the newcomer instead.

The two implementations must agree on the lock file naming, or they do not
exclude each other.
"""

from __future__ import annotations

import fcntl
import logging
import os
import socket
from pathlib import Path

logger = logging.getLogger(__name__)

_IPC_SCHEME = "ipc://"
_LOCK_SUFFIX = ".lock"
_LOCK_MODE = 0o600


def endpoint_path(endpoint: str) -> Path | None:
    """Filesystem path an ipc:// endpoint binds to, or None for other transports."""
    if not endpoint.startswith(_IPC_SCHEME):
        return None
    return Path(endpoint.removeprefix(_IPC_SCHEME))


def has_live_owner(endpoint: str) -> bool:
    """Whether another process is currently accepting on ``endpoint``.

    libzmq's ipc:// transport is AF_UNIX/SOCK_STREAM, so a plain connect() is a
    valid liveness probe with no ZMQ machinery involved: a path left behind by a
    killed process refuses the connection, a live listener accepts it.

    Every "cannot tell" answer is reported as live, so the caller never binds
    over something it does not understand. Refusing to start is recoverable;
    silently splitting the bus in two is not.

    On its own this is racy -- another process can bind between the check and
    the bind. :class:`EndpointLock` closes that window for anything using the
    same lock; this remains the best available answer for an owner that is not.
    """
    path = endpoint_path(endpoint)
    if path is None or not path.exists():
        return False
    if not path.is_socket():
        logger.warning("Endpoint path exists and is not a socket: %s", path)
        return True

    probe = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        probe.connect(str(path))
    except ConnectionRefusedError:
        return False  # Nobody listening: the owner is gone.
    except OSError:
        return True  # Cannot tell; assume live.
    else:
        return True  # Someone answered.
    finally:
        probe.close()


class EndpointLock:
    """Exclusive advisory ownership of a bind endpoint, held until released.

    This is what makes "may I bind here" atomic. A liveness probe cannot be:
    probing and binding are two separate calls, and another process can bind in
    between. flock is arbitrated by the kernel, so that window does not exist.

    It is also crash-safe, which an ``O_EXCL`` lock file is not: the lock lives
    on the open file description and the kernel drops it when the fd closes --
    including when the process dies -- so a killed run leaves nothing behind
    that would block the next one.
    """

    def __init__(self) -> None:
        self._fd: int | None = None

    def try_acquire(self, endpoint: str) -> bool:
        """Take the lock guarding ``endpoint``.

        Returns False if another live process holds it. Endpoints with no
        lockable path succeed trivially.
        """
        path = endpoint_path(endpoint)
        if path is None:
            return True  # No filesystem path to guard.

        lock_path = path.parent / (path.name + _LOCK_SUFFIX)
        try:
            fd = os.open(lock_path, os.O_CREAT | os.O_RDWR | os.O_CLOEXEC, _LOCK_MODE)
        except OSError:
            # Cannot lock here -- a read-only directory, for instance. Fall
            # through to the liveness probe rather than refusing to start over
            # a missing luxury.
            logger.warning("Could not open lock file %s", lock_path)
            return True

        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            os.close(fd)
            return False

        self._fd = fd
        return True

    def release(self) -> None:
        """Release the lock. Idempotent.

        The lock file itself is deliberately left behind. Unlinking it would
        reopen the race it exists to close: one process removing the file
        another has already opened leaves the two holding locks on different
        inodes, both believing they won.
        """
        if self._fd is not None:
            os.close(self._fd)
            self._fd = None
