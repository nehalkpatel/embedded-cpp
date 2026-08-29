"""Integration tests for I2C test application."""

import subprocess
from typing import Any

import pytest

from host_emulator import DeviceEmulator, PinState

# Mirror the address and pattern i2c_demo writes and verifies
# (src/apps/i2c_demo/i2c_demo.cpp is the source of truth).
DEVICE_ADDRESS = 0x50
TEST_PATTERN = [0xDE, 0xAD, 0xBE, 0xEF]


def test_i2c_demo_starts(i2c_demo: subprocess.Popen[bytes]) -> None:
    """Test that i2c_demo starts successfully."""
    assert i2c_demo.poll() is None, "i2c_demo process terminated unexpectedly"


@pytest.mark.usefixtures("i2c_demo")
def test_i2c_demo_write_read_cycle(emulator: DeviceEmulator) -> None:
    """Test that i2c_demo writes and reads from I2C device."""
    write_count = 0
    read_count = 0

    def i2c_handler(message: dict[str, Any]) -> None:
        nonlocal write_count, read_count
        if message.get("operation") == "Send":
            write_count += 1
            data = message.get("data", [])
            address = message.get("address", 0)

            assert address == DEVICE_ADDRESS, f"Wrong address: 0x{address:02X}"
            assert data == TEST_PATTERN, f"Wrong data: {data}"

        elif message.get("operation") == "Receive":
            read_count += 1
            address = message.get("address", 0)
            assert address == DEVICE_ADDRESS, f"Wrong address: 0x{address:02X}"

    emulator.i2c1().set_on_request(i2c_handler)

    emulator.i2c1().write_to_device(DEVICE_ADDRESS, TEST_PATTERN)

    assert emulator.i2c1().wait_for_transactions(
        2, address=DEVICE_ADDRESS, timeout=3.0
    ), "No I2C transactions occurred within timeout"

    assert write_count > 0, "No I2C writes occurred"
    assert read_count > 0, "No I2C reads occurred"
    assert write_count == read_count, (
        f"Write/read mismatch: {write_count} writes, {read_count} reads"
    )


@pytest.mark.usefixtures("i2c_demo")
def test_i2c_demo_toggles_leds(emulator: DeviceEmulator) -> None:
    """Test that i2c_demo toggles LEDs based on I2C operations."""
    emulator.i2c1().write_to_device(DEVICE_ADDRESS, TEST_PATTERN)

    assert emulator.user_led1().wait_for_operation("Set", timeout=2.0), (
        "LED1 didn't change state"
    )
    assert emulator.user_led2().wait_for_operation("Set", timeout=2.0), (
        "LED2 didn't change state"
    )


@pytest.mark.usefixtures("i2c_demo")
def test_i2c_demo_data_mismatch(emulator: DeviceEmulator) -> None:
    """Test that i2c_demo handles data mismatch correctly."""
    wrong_pattern = [0x00, 0x11, 0x22, 0x33]

    emulator.i2c1().write_to_device(DEVICE_ADDRESS, wrong_pattern)

    assert emulator.i2c1().wait_for_operation(
        "Receive", address=DEVICE_ADDRESS, timeout=2.0
    ), "No I2C read occurred"

    assert emulator.user_led1().wait_for_state(PinState.Low, timeout=2.0), (
        "LED1 should be Low due to data mismatch"
    )

    assert emulator.user_led2().wait_for_transitions(1, timeout=2.0), (
        "LED2 (heartbeat) didn't toggle"
    )
