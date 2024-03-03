from emulator import DeviceEmulator, Pin
from time import sleep
import pathlib
import subprocess
import pytest


pin_stats = {}


# Emulator must be stopped manually within each test
@pytest.fixture
def emulator(request):
    device_emulator = DeviceEmulator()
    device_emulator.start()

    yield device_emulator

    if device_emulator.running:
        print("[Fixture] Stopping emulator")
        device_emulator.stop()


# Blinky must be stopped manually within each test
@pytest.fixture
def blinky(request):
    blinky_executable = pathlib.Path("blinky").resolve()
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


def pin_stats_handler(message):
    name = message["name"]
    state = message["state"]
    print(f"[Test] {name} Handler Received request: {message}")
    if name not in pin_stats:
        pin_stats[name] = {}
    if "operation" in message:
        operation = message["operation"]
        pin_stats[name][operation] = pin_stats[name].get(operation, 0) + 1
    pin_stats[name][state] = pin_stats[name].get(state, 0) + 1


def test_blinky_start_stop(emulator, blinky):
    try:
        pin_stats.clear()
        assert emulator is not None
        assert blinky is not None
        assert emulator.running

    finally:
        emulator.stop()
        blinky.terminate()
        blinky.wait(timeout=1)


def test_blinky_blink(emulator, blinky):
    try:
        pin_stats.clear()
        emulator.UserLed1().set_on_request(pin_stats_handler)
        emulator.UserLed2().set_on_request(pin_stats_handler)

        sleep(0.75)

        assert pin_stats["LED 1"]["Set"] > 0
        assert pin_stats["LED 1"]["Get"] > 0
        assert pin_stats["LED 1"]["Low"] > 0
        assert pin_stats["LED 1"]["High"] > 0

        assert "LED 2" not in pin_stats
    finally:
        emulator.stop()
        blinky.terminate()
        blinky.wait(timeout=1)


def test_blinky_button_press(emulator, blinky):
    # Blinky is configured to set LED2 to high on a rising edge for Button1
    try:
        pin_stats.clear()
        emulator.UserLed2().set_on_request(pin_stats_handler)
        emulator.UserButton1().set_on_response(pin_stats_handler)

        emulator.UserButton1().set_state(Pin.state.Low)
        emulator.UserButton1().set_state(Pin.state.High)

        assert pin_stats["Button 1"]["Low"] == 1
        assert pin_stats["Button 1"]["High"] == 1

        assert "Get" not in pin_stats["LED 2"]
        assert "Low" not in pin_stats["LED 2"]
        assert pin_stats["LED 2"]["Set"] == 1
        assert pin_stats["LED 2"]["High"] == 1

    finally:
        emulator.stop()
        blinky.terminate()
        blinky.wait(timeout=1)
