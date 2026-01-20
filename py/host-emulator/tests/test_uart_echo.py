"""Integration tests for UART echo application with RxHandler."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    import subprocess

    from host_emulator import DeviceEmulator


def test_uart_echo_starts(
    emulator: DeviceEmulator, uart_echo: subprocess.Popen[bytes]
) -> None:
    """Test that uart_echo starts successfully."""
    _ = emulator  # Ensure emulator is running
    assert uart_echo.poll() is None, "uart_echo process terminated unexpectedly"


def test_uart_echo_sends_greeting(
    emulator: DeviceEmulator, uart_echo: subprocess.Popen[bytes]
) -> None:
    """Test that uart_echo sends a greeting message on startup."""
    _ = uart_echo  # Ensure uart_echo is running
    assert emulator.uart1().wait_for_data(min_bytes=1, timeout=2.0), (
        "No greeting received from uart_echo"
    )

    greeting = bytes(emulator.uart1().rx_buffer).decode("utf-8", errors="ignore")
    assert "UART Echo ready" in greeting, f"Unexpected greeting: {greeting}"


def test_uart_echo_echoes_data(
    emulator: DeviceEmulator, uart_echo: subprocess.Popen[bytes]
) -> None:
    """Test that uart_echo echoes received data back."""
    _ = uart_echo  # Ensure uart_echo is running
    emulator.uart1().rx_buffer.clear()

    test_data = [0x48, 0x65, 0x6C, 0x6C, 0x6F]  # "Hello"
    response = emulator.uart1().send_data(test_data)

    assert response["status"] == "Ok"

    assert emulator.uart1().wait_for_data(min_bytes=len(test_data), timeout=1.0), (
        "Echo data not received within timeout"
    )

    assert len(emulator.uart1().rx_buffer) == len(test_data)
    assert list(emulator.uart1().rx_buffer) == test_data


def test_uart_echo_handler_receives_echoed_data(
    emulator: DeviceEmulator, uart_echo: subprocess.Popen[bytes]
) -> None:
    """Test that UART handler callback is invoked when device sends data."""
    _ = uart_echo  # Ensure uart_echo is running
    emulator.uart1().rx_buffer.clear()

    received_via_handler: list[int] = []

    def uart_handler(message: dict[str, Any]) -> None:
        if message.get("operation") == "Send":
            data = message.get("data", [])
            received_via_handler.extend(data)

    emulator.uart1().set_on_request(uart_handler)

    test_data = [0x54, 0x65, 0x73, 0x74]  # "Test"
    response = emulator.uart1().send_data(test_data)

    assert response["status"] == "Ok"

    assert emulator.uart1().wait_for_data(min_bytes=len(test_data), timeout=1.0), (
        "Echo data not received within timeout"
    )

    assert len(received_via_handler) == len(test_data), (
        f"Handler received {len(received_via_handler)} bytes, expected {len(test_data)}"
    )
    assert received_via_handler == test_data, (
        f"Handler received {received_via_handler}, expected {test_data}"
    )
