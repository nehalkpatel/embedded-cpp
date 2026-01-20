"""Integration tests for blinky application."""

from __future__ import annotations

from typing import TYPE_CHECKING

from host_emulator import DeviceEmulator, PinState

if TYPE_CHECKING:
    import subprocess


def test_blinky_start_stop(
    emulator: DeviceEmulator, blinky: subprocess.Popen[bytes]
) -> None:
    """Test that blinky starts and stops cleanly."""
    assert emulator is not None
    assert blinky is not None
    assert emulator.running


def test_blinky_blink(
    emulator: DeviceEmulator, blinky: subprocess.Popen[bytes]
) -> None:
    """Test that blinky blinks LED1."""
    _ = blinky  # Ensure blinky is running
    assert emulator.user_led1().wait_for_transitions(2, timeout=3.0), (
        "LED1 didn't blink within timeout"
    )


def test_blinky_button_press(
    emulator: DeviceEmulator, blinky: subprocess.Popen[bytes]
) -> None:
    """Test that button press triggers LED2."""
    _ = blinky  # Ensure blinky is running
    emulator.user_button1().set_state(PinState.Low)
    emulator.user_button1().set_state(PinState.High)

    assert emulator.user_led2().wait_for_state(PinState.High, timeout=1.0), (
        "LED2 didn't turn on after button press"
    )
