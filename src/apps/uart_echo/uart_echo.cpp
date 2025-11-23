#include "uart_echo.hpp"

#include <array>
#include <chrono>
#include <expected>
#include <functional>

#include "apps/app.hpp"
#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/delay.hpp"
#include "libs/mcu/uart.hpp"

namespace app {
using std::chrono::operator""ms;

auto AppMain(board::Board& board) -> std::expected<void, common::Error> {
  UartEcho uart_echo{board};
  if (!uart_echo.Init()) {
    return std::unexpected(common::Error::kUnknown);
  }
  if (!uart_echo.Run()) {
    return std::unexpected(common::Error::kUnknown);
  }
  return {};
}

auto UartEcho::Init() -> std::expected<void, common::Error> {
  // Initialize the board
  auto status = board_.Init();
  if (!status) {
    return status;
  }

  // Initialize UART
  const mcu::UartConfig uart_config{};
  auto uart_init = board_.Uart1().Init(uart_config);
  if (!uart_init) {
    return uart_init;
  }

  // Set up RxHandler to echo received data back and toggle LED
  auto handler_result =
      board_.Uart1().SetRxHandler([this](const uint8_t* data, size_t size) {
        // Echo the data back
        std::vector<uint8_t> echo_data(data, data + size);
        std::ignore = board_.Uart1().Send(echo_data);

        // Toggle LED1 to indicate data received
        auto led_state = board_.UserLed1().Get();
        if (led_state && led_state.value() == mcu::PinState::kHigh) {
          std::ignore = board_.UserLed1().SetLow();
        } else {
          std::ignore = board_.UserLed1().SetHigh();
        }
      });

  return handler_result;
}

auto UartEcho::Run() -> std::expected<void, common::Error> {
  // Send initial greeting message
  const std::string greeting = "UART Echo ready! Send data to echo it back.\n";
  auto send_result = board_.Uart1().Send(
      std::vector<uint8_t>(greeting.begin(), greeting.end()));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  // Main loop - just blink LED2 slowly to show we're alive
  // The actual echo happens via the RxHandler callback
  while (true) {
    mcu::Delay(1000ms);
    auto led_state = board_.UserLed2().Get();
    if (!led_state) {
      return std::unexpected(led_state.error());
    }
    if (led_state.value() == mcu::PinState::kHigh) {
      auto status = board_.UserLed2().SetLow();
      if (!status) {
        return std::unexpected(status.error());
      }
    } else {
      auto status = board_.UserLed2().SetHigh();
      if (!status) {
        return std::unexpected(status.error());
      }
    }
  }
  return {};
}

}  // namespace app
