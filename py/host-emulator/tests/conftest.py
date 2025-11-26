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


@pytest.fixture(scope="function")
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


def _wait_for_process_ready(process, timeout=2.0):
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


@pytest.fixture(scope="function")
def blinky(request, emulator):
    """Start blinky application after emulator is ready."""
    blinky_arg = request.config.getoption("--blinky")
    if not blinky_arg:
        pytest.skip("--blinky not provided")

    blinky_executable = pathlib.Path(blinky_arg).resolve()
    assert blinky_executable.exists(), (
        f"Blinky executable not found: {blinky_executable}"
    )

    # Emulator is already started and ready (fixture dependency)
    blinky_process = subprocess.Popen(
        [str(blinky_executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    try:
        # Wait for blinky to be ready
        _wait_for_process_ready(blinky_process)

        yield blinky_process

    finally:
        # Automatic cleanup
        if blinky_process.poll() is None:
            logger.debug("[Fixture] Stopping blinky")
            blinky_process.terminate()
            try:
                blinky_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                blinky_process.kill()
                blinky_process.wait()
        logger.debug(f"[Fixture] Blinky exit code: {blinky_process.returncode}")


@pytest.fixture(scope="function")
def uart_echo(request, emulator):
    """Start uart_echo application after emulator is ready."""
    uart_echo_arg = request.config.getoption("--uart-echo")
    if not uart_echo_arg:
        pytest.skip("--uart-echo not provided")

    uart_echo_executable = pathlib.Path(uart_echo_arg).resolve()
    assert uart_echo_executable.exists()

    uart_echo_process = subprocess.Popen(
        [str(uart_echo_executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    try:
        _wait_for_process_ready(uart_echo_process)
        yield uart_echo_process
    finally:
        if uart_echo_process.poll() is None:
            logger.debug("[Fixture] Stopping uart_echo")
            uart_echo_process.terminate()
            try:
                uart_echo_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                uart_echo_process.kill()
                uart_echo_process.wait()
        logger.debug(f"[Fixture] UartEcho exit code: {uart_echo_process.returncode}")


@pytest.fixture(scope="function")
def i2c_demo(request, emulator):
    """Start i2c_demo application after emulator is ready."""
    i2c_demo_arg = request.config.getoption("--i2c-demo")
    if not i2c_demo_arg:
        pytest.skip("--i2c-demo not provided")

    i2c_demo_executable = pathlib.Path(i2c_demo_arg).resolve()
    assert i2c_demo_executable.exists()

    i2c_demo_process = subprocess.Popen(
        [str(i2c_demo_executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    try:
        _wait_for_process_ready(i2c_demo_process)
        yield i2c_demo_process
    finally:
        if i2c_demo_process.poll() is None:
            logger.debug("[Fixture] Stopping i2c_demo")
            i2c_demo_process.terminate()
            try:
                i2c_demo_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                i2c_demo_process.kill()
                i2c_demo_process.wait()
        logger.debug(f"[Fixture] I2CDemo exit code: {i2c_demo_process.returncode}")
