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
def emulator() -> Generator[DeviceEmulator, None, None]:
    """Start emulator and ensure it's ready before returning."""
    device_emulator = DeviceEmulator()

    try:
        device_emulator.start()
        yield device_emulator

    finally:
        if device_emulator.running:
            logger.debug("[Fixture] Stopping emulator")
            device_emulator.stop()


def _wait_for_process_ready(
    process: subprocess.Popen[bytes], timeout: float = 1.0
) -> None:
    """Wait for process to be running and responsive."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        if process.poll() is not None:
            raise RuntimeError(f"Process exited with code {process.returncode}")
        time.sleep(0.1)
    time.sleep(0.1)


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
    ) -> Generator[subprocess.Popen[bytes], None, None]:
        """Start application after emulator is ready."""
        _ = emulator  # Ensure emulator is started first
        app_arg = request.config.getoption(option_name)
        if not app_arg:
            pytest.skip(f"{option_name} not provided")

        app_executable = Path(str(app_arg)).resolve()
        assert app_executable.exists(), (
            f"{display_name} executable not found: {app_executable}"
        )

        app_process = subprocess.Popen(
            [str(app_executable)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

        try:
            _wait_for_process_ready(app_process)
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
