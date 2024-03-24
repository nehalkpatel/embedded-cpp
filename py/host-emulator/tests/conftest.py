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
