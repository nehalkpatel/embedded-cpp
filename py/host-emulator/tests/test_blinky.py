from time import sleep

from host_emulator import Pin

pin_stats = {}


def pin_stats_handler(message):
    name = message["name"]
    state = message["state"]
    if name not in pin_stats:
        pin_stats[name] = {}
    if "operation" in message:
        operation = message["operation"]
        pin_stats[name][operation] = pin_stats[name].get(operation, 0) + 1
    pin_stats[name][state] = pin_stats[name].get(state, 0) + 1


def test_blinky_start_stop(emulator, blinky):
    """Test that blinky starts and stops cleanly."""
    pin_stats.clear()
    assert emulator is not None
    assert blinky is not None
    assert emulator.running


def test_blinky_blink(emulator, blinky):
    """Test that blinky blinks LED1."""
    pin_stats.clear()
    emulator.user_led1().set_on_request(pin_stats_handler)
    emulator.user_led2().set_on_request(pin_stats_handler)

    sleep(1.75)

    assert pin_stats["LED 1"]["Set"] > 0
    assert pin_stats["LED 1"]["Get"] > 0
    assert pin_stats["LED 1"]["Low"] > 0
    assert pin_stats["LED 1"]["High"] > 0
    assert "LED 2" not in pin_stats


def test_blinky_button_press(emulator, blinky):
    """Test that button press triggers LED2."""
    pin_stats.clear()
    emulator.user_led2().set_on_request(pin_stats_handler)
    emulator.user_button1().set_on_response(pin_stats_handler)

    emulator.user_button1().set_state(Pin.state.Low)
    emulator.user_button1().set_state(Pin.state.High)

    assert pin_stats["Button 1"]["Low"] == 1
    assert pin_stats["Button 1"]["High"] == 1
    assert "Get" not in pin_stats["LED 2"]
    assert "Low" not in pin_stats["LED 2"]
    assert pin_stats["LED 2"]["Set"] == 1
    assert pin_stats["LED 2"]["High"] == 1
