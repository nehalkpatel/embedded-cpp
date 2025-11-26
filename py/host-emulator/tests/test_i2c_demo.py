"""Integration tests for I2C test application."""

import time


def test_i2c_demo_starts(emulator, i2c_demo):
    """Test that i2c_demo starts successfully."""
    # Give i2c_demo time to initialize
    time.sleep(0.5)

    # Check that the process is still running
    assert i2c_demo.poll() is None, "i2c_demo process terminated unexpectedly"


def test_i2c_demo_write_read_cycle(emulator, i2c_demo):
    """Test that i2c_demo writes and reads from I2C device."""
    device_address = 0x50
    test_pattern = [0xDE, 0xAD, 0xBE, 0xEF]
    write_count = 0
    read_count = 0

    def i2c_handler(message):
        nonlocal write_count, read_count
        if message.get("operation") == "Send":
            # Device is writing to I2C peripheral
            write_count += 1
            data = message.get("data", [])
            address = message.get("address", 0)

            # Verify the data and address
            assert address == device_address, f"Wrong address: 0x{address:02X}"
            assert data == test_pattern, f"Wrong data: {data}"

        elif message.get("operation") == "Receive":
            # Device is reading from I2C peripheral
            read_count += 1
            address = message.get("address", 0)
            assert address == device_address, f"Wrong address: 0x{address:02X}"

    emulator.i2c1().set_on_request(i2c_handler)

    # Pre-populate I2C device buffer with test pattern
    emulator.i2c1().write_to_device(device_address, test_pattern)

    # Give i2c_demo time to run a few cycles
    time.sleep(1.5)

    # Verify that writes and reads occurred
    assert write_count > 0, "No I2C writes occurred"
    assert read_count > 0, "No I2C reads occurred"
    assert write_count == read_count, (
        f"Write/read mismatch: {write_count} writes, {read_count} reads"
    )


def test_i2c_demo_toggles_leds(emulator, i2c_demo):
    """Test that i2c_demo toggles LEDs based on I2C operations."""
    device_address = 0x50
    test_pattern = [0xDE, 0xAD, 0xBE, 0xEF]

    # Pre-populate I2C device buffer with correct test pattern
    emulator.i2c1().write_to_device(device_address, test_pattern)

    # Give i2c_demo time to initialize
    time.sleep(0.5)

    # Record initial LED states
    initial_led1 = emulator.get_pin_state("LED 1")
    initial_led2 = emulator.get_pin_state("LED 2")

    # Wait for exactly one more toggle cycle (~550ms per cycle)
    time.sleep(0.4)

    # Check that LEDs have toggled
    final_led1 = emulator.get_pin_state("LED 1")
    final_led2 = emulator.get_pin_state("LED 2")

    # LED2 should have toggled (heartbeat)
    assert final_led2 != initial_led2, (
        f"LED2 didn't toggle: {initial_led2} -> {final_led2}"
    )

    # LED1 should have toggled (data verification success)
    assert final_led1 != initial_led1, (
        f"LED1 didn't toggle: {initial_led1} -> {final_led1}"
    )


def test_i2c_demo_data_mismatch(emulator, i2c_demo):
    """Test that i2c_demo handles data mismatch correctly."""
    device_address = 0x50
    wrong_pattern = [0x00, 0x11, 0x22, 0x33]  # Different from test pattern

    # Pre-populate I2C device buffer with wrong data
    emulator.i2c1().write_to_device(device_address, wrong_pattern)

    # Give i2c_demo time to run a few cycles
    time.sleep(1.2)

    # LED1 should be off due to data mismatch
    led1_state = emulator.get_pin_state("LED 1")
    assert led1_state.name == "Low", f"LED1 should be off, but is {led1_state.name}"

    # LED2 should still be blinking (alive indicator)
    initial_led2 = emulator.get_pin_state("LED 2")
    time.sleep(0.6)
    final_led2 = emulator.get_pin_state("LED 2")
    assert final_led2 != initial_led2, (
        f"LED2 didn't toggle: {initial_led2} -> {final_led2}"
    )
