from emulator import DeviceEmulator
from pytest import fixture
import pathlib
import subprocess


def pytest_addoption(parser):
    parser.addoption(
        "--blinky",
        action="store",
        default=None,
        help="Path to the blinky executable",
    )
    parser.addoption(
        "--uart-echo",
        action="store",
        default=None,
        help="Path to the uart_echo executable",
    )


# Emulator must be stopped manually within each test
@fixture
def emulator(request):
    device_emulator = DeviceEmulator()
    device_emulator.start()

    yield device_emulator

    if device_emulator.running:
        print("[Fixture] Stopping emulator")
        device_emulator.stop()


# Blinky must be stopped manually within each test
@fixture()
def blinky(request):
    blinky_arg = request.config.getoption("--blinky")
    blinky_executable = pathlib.Path(blinky_arg).resolve()
    assert blinky_executable.exists()
    blinky_process = subprocess.Popen(
        [str(blinky_executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    yield blinky_process

    if blinky_process.poll() is None:
        print("[Fixture] Stopping blinky")
        blinky_process.kill()
        blinky_process.wait(timeout=1)
        print(f"[Fixture] Blinky return code: {blinky_process.returncode}")


# UartEcho must be stopped manually within each test
@fixture()
def uart_echo(request):
    uart_echo_arg = request.config.getoption("--uart-echo")
    uart_echo_executable = pathlib.Path(uart_echo_arg).resolve()
    assert uart_echo_executable.exists()
    uart_echo_process = subprocess.Popen(
        [str(uart_echo_executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    yield uart_echo_process

    if uart_echo_process.poll() is None:
        print("[Fixture] Stopping uart_echo")
        uart_echo_process.kill()
        uart_echo_process.wait(timeout=1)
        print(f"[Fixture] UartEcho return code: {uart_echo_process.returncode}")
