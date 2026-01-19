#include "uart_echo.hpp"

#include <array>
#include <chrono>
#include <expected>
#include <functional>
#include <span>

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
  const mcu::UartConfig uart_config{};

  return board_.Init()
      .and_then(
          [this, &uart_config]() { return board_.Uart1().Init(uart_config); })
      .and_then([this]() {
        return board_.Uart1().SetRxHandler(
            [this](const std::byte* data, size_t size) {
              // Echo the data back
              const std::vector<std::byte> echo_data(data, data + size);
              std::ignore = board_.Uart1().Send(echo_data);

              // Toggle LED1 to indicate data received
              std::ignore = board_.UserLed1().Toggle();
            });
      });
}

auto UartEcho::Run() -> std::expected<void, common::Error> {
  // Send initial greeting message
  const std::string greeting{"UART Echo ready! Send data to echo it back.\n"};
  auto send_result{board_.Uart1().Send(std::as_bytes(std::span{greeting}))};
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  // Main loop - just blink LED2 slowly to show we're alive
  // The actual echo happens via the RxHandler callback
  while (true) {
    mcu::Delay(200ms);
    std::ignore = board_.UserLed2().Toggle();
  }
  return {};
}

}  // namespace app
