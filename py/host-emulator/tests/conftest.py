import logging
import pathlib
import subprocess
import time

import pytest
from host_emulator import DeviceEmulator

# Configure logging for tests
logger = logging.getLogger(__name__)


def pytest_addoption(parser):
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
def emulator(request):
    """Start emulator and ensure it's ready before returning."""
    device_emulator = DeviceEmulator()

    try:
        # Start emulator (now waits until ready)
        device_emulator.start()

        yield device_emulator

    finally:
        # Automatic cleanup - tests don't need to call stop()
        if device_emulator.running:
            logger.debug("[Fixture] Stopping emulator")
            device_emulator.stop()


def _wait_for_process_ready(process, timeout=1.0):
    """Wait for process to be running and responsive."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        if process.poll() is not None:
            # Process exited - something went wrong
            raise RuntimeError(f"Process exited with code {process.returncode}")
        time.sleep(0.1)
    # Give a bit more time for ZMQ connections
    # Reduced from 0.2s since ZmqTransport constructor already has 10ms sleep
    time.sleep(0.1)


def _application_fixture_factory(option_name, display_name):
    """Factory function to create application fixtures with common lifecycle management.

    Args:
        option_name: CLI option name (e.g., "--blinky")
        display_name: Display name for logging (e.g., "Blinky")

    Returns:
        A pytest fixture function
    """

    @pytest.fixture(scope="module")
    def application_fixture(request, emulator):
        """Start application after emulator is ready."""
        app_arg = request.config.getoption(option_name)
        if not app_arg:
            pytest.skip(f"{option_name} not provided")

        app_executable = pathlib.Path(app_arg).resolve()
        assert app_executable.exists(), (
            f"{display_name} executable not found: {app_executable}"
        )

        # Emulator is already started and ready (fixture dependency)
        app_process = subprocess.Popen(
            [str(app_executable)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

        try:
            # Wait for application to be ready
            _wait_for_process_ready(app_process)
            yield app_process

        finally:
            # Automatic cleanup
            if app_process.poll() is None:
                logger.debug(f"[Fixture] Stopping {display_name}")
                app_process.terminate()
                try:
                    app_process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    app_process.kill()
                    app_process.wait()
            logger.debug(
                f"[Fixture] {display_name} exit code: {app_process.returncode}"
            )

    return application_fixture


# Create application fixtures using the factory
blinky = _application_fixture_factory("--blinky", "Blinky")
uart_echo = _application_fixture_factory("--uart-echo", "UartEcho")
i2c_demo = _application_fixture_factory("--i2c-demo", "I2CDemo")
