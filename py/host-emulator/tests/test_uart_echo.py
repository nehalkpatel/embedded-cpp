"""Integration tests for UART echo application with RxHandler."""


def test_uart_echo_starts(emulator, uart_echo):
    """Test that uart_echo starts successfully."""
    # Check that the process is still running
    assert uart_echo.poll() is None, "uart_echo process terminated unexpectedly"


def test_uart_echo_sends_greeting(emulator, uart_echo):
    """Test that uart_echo sends a greeting message on startup."""
    # Wait for UART to receive greeting data
    assert emulator.uart1().wait_for_data(min_bytes=1, timeout=2.0), (
        "No greeting received from uart_echo"
    )

    # Check that the greeting contains expected text
    greeting = bytes(emulator.uart1().rx_buffer).decode("utf-8", errors="ignore")
    assert "UART Echo ready" in greeting, f"Unexpected greeting: {greeting}"


def test_uart_echo_echoes_data(emulator, uart_echo):
    """Test that uart_echo echoes received data back."""
    # Clear any initial greeting data
    emulator.uart1().rx_buffer.clear()

    # Send data to the device
    test_data = [0x48, 0x65, 0x6C, 0x6C, 0x6F]  # "Hello"
    response = emulator.uart1().send_data(test_data)

    # Verify the response acknowledges receipt
    assert response["status"] == "Ok"

    # Wait for the RxHandler to process and echo back
    assert emulator.uart1().wait_for_data(min_bytes=len(test_data), timeout=1.0), (
        "Echo data not received within timeout"
    )

    # Check that the data was echoed back
    assert len(emulator.uart1().rx_buffer) == len(test_data)
    assert list(emulator.uart1().rx_buffer) == test_data


def test_uart_echo_handler_receives_echoed_data(emulator, uart_echo):
    """Test that UART handler callback is invoked when device sends data."""
    # Clear any initial greeting data
    emulator.uart1().rx_buffer.clear()

    # Track data received via handler
    received_via_handler = []

    def uart_handler(message):
        if message.get("operation") == "Send":
            data = message.get("data", [])
            received_via_handler.extend(data)

    # Register handler BEFORE sending data
    emulator.uart1().set_on_request(uart_handler)

    # Send data to the device
    test_data = [0x54, 0x65, 0x73, 0x74]  # "Test"
    response = emulator.uart1().send_data(test_data)

    # Verify the response acknowledges receipt
    assert response["status"] == "Ok"

    # Wait for the RxHandler to process and echo back
    assert emulator.uart1().wait_for_data(min_bytes=len(test_data), timeout=1.0), (
        "Echo data not received within timeout"
    )

    # Verify handler was called with echoed data
    assert len(received_via_handler) == len(test_data), (
        f"Handler received {len(received_via_handler)} bytes, expected {len(test_data)}"
    )
    assert received_via_handler == test_data, (
        f"Handler received {received_via_handler}, expected {test_data}"
    )
