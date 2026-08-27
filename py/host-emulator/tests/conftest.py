"""Pytest configuration and fixtures for host-emulator tests."""

from __future__ import annotations

import logging
import subprocess
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

import pytest

from host_emulator import DeviceEmulator

if TYPE_CHECKING:
    from collections.abc import Generator

logger = logging.getLogger(__name__)


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--blinky", action="store", default=None, help="Path to the blinky executable"
    )
    parser.addoption(
        "--uart-echo",
        action="store",
        default=None,
        help="Path to the uart_echo executable",
    )
    parser.addoption(
        "--i2c-demo",
        action="store",
        default=None,
        help="Path to the i2c_demo executable",
    )


@pytest.fixture(scope="module")
def emulator() -> Generator[DeviceEmulator]:
    """Start emulator and ensure it's ready before returning."""
    device_emulator = DeviceEmulator()

    try:
        device_emulator.start()
        yield device_emulator

    finally:
        if device_emulator.running:
            logger.debug("[Fixture] Stopping emulator")
            device_emulator.stop()


def _endpoint_path(endpoint: str) -> Path | None:
    """Filesystem path an ipc:// endpoint binds to, or None for other transports."""
    if not endpoint.startswith("ipc://"):
        return None
    return Path(endpoint.removeprefix("ipc://"))


def _wait_for_process_ready(
    process: subprocess.Popen[bytes],
    ready_path: Path | None,
    timeout: float = 5.0,
    poll_interval: float = 0.01,
) -> None:
    """Block until the application has bound its receive endpoint.

    Readiness is the appearance of the app's ipc socket file: the C++ transport
    binds it inside ZmqTransport::Create(), before Create() returns.

    This is narrower than "the app is ready". It does not prove the app finished
    connecting to the emulator, nor that it reached its main loop -- both happen
    after the bind and neither is observable from here. The per-test wait_for_*
    helpers remain the real synchronisation for those.

    The previous version had no success exit at all: it slept out its full
    timeout on every call and treated "did not die" as ready.
    """
    if ready_path is None:
        return  # No observable readiness signal for non-ipc endpoints.

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Process exited with code {process.returncode}")
        if ready_path.exists():
            return
        time.sleep(poll_interval)

    raise RuntimeError(f"Process did not bind {ready_path} within {timeout}s")


def _application_fixture_factory(option_name: str, display_name: str) -> Any:
    """Factory function to create application fixtures with common lifecycle management.

    Args:
        option_name: CLI option name (e.g., "--blinky")
        display_name: Display name for logging (e.g., "Blinky")

    Returns:
        A pytest fixture function
    """

    @pytest.fixture(scope="module")
    def application_fixture(
        request: pytest.FixtureRequest, emulator: DeviceEmulator
    ) -> Generator[subprocess.Popen[bytes]]:
        """Start application after emulator is ready."""
        _ = emulator  # Ensure emulator is started first
        app_arg = request.config.getoption(option_name)
        if not app_arg:
            pytest.skip(f"{option_name} not provided")

        app_executable = Path(str(app_arg)).resolve()
        assert app_executable.exists(), (
            f"{display_name} executable not found: {app_executable}"
        )

        # Clear any leftover socket file first, so its later appearance is
        # evidence of *this* run binding rather than of a previous one.
        ready_path = _endpoint_path(emulator.to_device_endpoint)
        if ready_path is not None:
            ready_path.unlink(missing_ok=True)

        app_process = subprocess.Popen(
            [str(app_executable)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

        try:
            _wait_for_process_ready(app_process, ready_path)
            yield app_process

        finally:
            if app_process.poll() is None:
                logger.debug("[Fixture] Stopping %s", display_name)
                app_process.terminate()
                try:
                    app_process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    app_process.kill()
                    app_process.wait()
            logger.debug(
                "[Fixture] %s exit code: %s", display_name, app_process.returncode
            )

    return application_fixture


# Create application fixtures using the factory
blinky = _application_fixture_factory("--blinky", "Blinky")
uart_echo = _application_fixture_factory("--uart-echo", "UartEcho")
i2c_demo = _application_fixture_factory("--i2c-demo", "I2CDemo")
