"""Integration tests for UART echo application with RxHandler."""

import time


def test_uart_echo_starts(emulator, uart_echo):
    """Test that uart_echo starts successfully."""
    try:
        # Give uart_echo time to initialize
        time.sleep(0.5)

        # Check that the process is still running
        assert uart_echo.poll() is None, "uart_echo process terminated unexpectedly"

    finally:
        emulator.stop()
        uart_echo.terminate()
        uart_echo.wait(timeout=1)


def test_uart_echo_sends_greeting(emulator, uart_echo):
    """Test that uart_echo sends a greeting message on startup."""
    try:
        received_data = []

        def uart_handler(message):
            if message.get("operation") == "Send":
                data = message.get("data", [])
                received_data.extend(data)

        emulator.Uart1().set_on_request(uart_handler)

        # Give uart_echo time to initialize and send greeting
        time.sleep(0.5)

        # Check that we received some data
        assert len(received_data) > 0, "No data received from UART"

        # Check that the greeting contains expected text
        greeting = bytes(received_data).decode("utf-8", errors="ignore")
        assert "UART Echo ready" in greeting, f"Unexpected greeting: {greeting}"

    finally:
        emulator.stop()
        uart_echo.terminate()
        uart_echo.wait(timeout=1)


def test_uart_echo_echoes_data(emulator, uart_echo):
    """Test that uart_echo echoes received data back."""
    try:
        # Give uart_echo time to initialize
        time.sleep(0.5)

        # Clear any initial greeting data
        emulator.Uart1().rx_buffer.clear()

        # Send data to the device
        test_data = [0x48, 0x65, 0x6C, 0x6C, 0x6F]  # "Hello"
        response = emulator.Uart1().send_data(test_data)

        # Verify the response acknowledges receipt
        assert response["status"] == "Ok"

        # Give time for the RxHandler to process and echo back
        time.sleep(0.2)

        # Check that the data was echoed back
        assert len(emulator.Uart1().rx_buffer) == len(test_data)
        assert list(emulator.Uart1().rx_buffer) == test_data

    finally:
        emulator.stop()
        uart_echo.terminate()
        uart_echo.wait(timeout=1)
