from host_emulator import Pin


def test_blinky_start_stop(emulator, blinky):
    """Test that blinky starts and stops cleanly."""
    assert emulator is not None
    assert blinky is not None
    assert emulator.running


def test_blinky_blink(emulator, blinky):
    """Test that blinky blinks LED1."""
    # Wait for LED1 to transition at least twice (one blink cycle)
    assert emulator.user_led1().wait_for_transitions(2, timeout=3.0), (
        "LED1 didn't blink within timeout"
    )


def test_blinky_button_press(emulator, blinky):
    """Test that button press triggers LED2."""
    # Trigger button press (rising edge)
    emulator.user_button1().set_state(Pin.state.Low)
    emulator.user_button1().set_state(Pin.state.High)

    # Wait for LED2 to turn on (high state)
    assert emulator.user_led2().wait_for_state(Pin.state.High, timeout=1.0), (
        "LED2 didn't turn on after button press"
    )
