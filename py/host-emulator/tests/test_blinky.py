from emulator import DeviceEmulator, Pin
from time import sleep
import pathlib
import subprocess
import pytest


user_led1_stats = {
    "high": 0,
    "low": 0,
    "get": 0,
    "set": 0,
}

user_button1_stats = {
    "pressed": 0,
    "released": 0,
    "get": 0,
    "set": 0,
}


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


def on_user_led1_request(message):
    if message["operation"] == "Get":
        user_led1_stats["get"] += 1

    if message["operation"] == "Set":
        user_led1_stats["set"] += 1

    if message["state"] == "High":
        user_led1_stats["high"] += 1

    if message["state"] == "Low":
        user_led1_stats["low"] += 1


def on_user_button1_response(message):
    print(f"[Test] Handler Received response: {message}")
    if message["state"] == "Low":
        user_button1_stats["pressed"] += 1

    if message["state"] == "High":
        user_button1_stats["released"] += 1


def test_blinky_start_stop(emulator, blinky):
    try:
        assert emulator is not None
        assert blinky is not None
        assert emulator.running

    finally:
        emulator.stop()
        blinky.terminate()
        blinky.wait(timeout=1)


def test_blinky_blink(emulator, blinky):
    try:
        emulator.UserLed1().set_on_request(on_user_led1_request)

        sleep(0.75)

        assert user_led1_stats["get"] > 0
        assert user_led1_stats["set"] > 0
        assert user_led1_stats["high"] > 0
        assert user_led1_stats["low"] > 0

    finally:
        emulator.stop()
        blinky.terminate()
        blinky.wait(timeout=1)


def test_blinky_button_press(emulator, blinky):
    try:
        emulator.UserButton1().set_on_response(on_user_button1_response)

        emulator.UserButton1().set_state(Pin.state.Low)
        emulator.UserButton1().set_state(Pin.state.High)

        assert user_button1_stats["pressed"] > 0
        assert user_button1_stats["released"] > 0

    finally:
        emulator.stop()
        blinky.terminate()
        blinky.wait(timeout=1)
