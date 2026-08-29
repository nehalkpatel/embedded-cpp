"""Integration tests for blinky application."""

import pytest

from host_emulator import DeviceEmulator, PinState


@pytest.mark.usefixtures("blinky")
def test_blinky_start_stop(emulator: DeviceEmulator) -> None:
    """Test that blinky starts and stops cleanly."""
    assert emulator.running


@pytest.mark.usefixtures("blinky")
def test_blinky_blink(emulator: DeviceEmulator) -> None:
    """Test that blinky blinks LED1."""
    assert emulator.user_led1().wait_for_transitions(2, timeout=3.0), (
        "LED1 didn't blink within timeout"
    )


@pytest.mark.usefixtures("blinky")
def test_blinky_button_press(emulator: DeviceEmulator) -> None:
    """Test that button press triggers LED2."""
    emulator.user_button1().set_state(PinState.Low)
    emulator.user_button1().set_state(PinState.High)

    assert emulator.user_led2().wait_for_state(PinState.High, timeout=1.0), (
        "LED2 didn't turn on after button press"
    )
