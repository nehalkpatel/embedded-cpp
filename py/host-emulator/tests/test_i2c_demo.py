"""Integration tests for I2C test application."""


def test_i2c_demo_starts(emulator, i2c_demo):
    """Test that i2c_demo starts successfully."""
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

    # Wait for at least one write/read transaction
    assert emulator.i2c1().wait_for_transactions(
        2, address=device_address, timeout=3.0
    ), "No I2C transactions occurred within timeout"

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

    # Wait for LED state changes (both should toggle)
    assert emulator.user_led1().wait_for_operation("Set", timeout=2.0), (
        "LED1 didn't change state"
    )
    assert emulator.user_led2().wait_for_operation("Set", timeout=2.0), (
        "LED2 didn't change state"
    )


def test_i2c_demo_data_mismatch(emulator, i2c_demo):
    """Test that i2c_demo handles data mismatch correctly."""
    device_address = 0x50
    wrong_pattern = [0x00, 0x11, 0x22, 0x33]  # Different from test pattern

    # Pre-populate I2C device buffer with wrong data
    emulator.i2c1().write_to_device(device_address, wrong_pattern)

    # Wait for at least one I2C transaction
    assert emulator.i2c1().wait_for_operation(
        "Receive", address=device_address, timeout=2.0
    ), "No I2C read occurred"

    # LED1 should be off (Low) due to data mismatch - wait for Set operation
    assert emulator.user_led1().wait_for_state(
        emulator.user_led1().state.Low, timeout=2.0
    ), "LED1 should be Low due to data mismatch"

    # LED2 should still be blinking (alive indicator) - wait for toggle
    assert emulator.user_led2().wait_for_transitions(1, timeout=2.0), (
        "LED2 (heartbeat) didn't toggle"
    )
