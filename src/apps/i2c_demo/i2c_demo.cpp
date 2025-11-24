#include "i2c_demo.hpp"

#include <array>
#include <chrono>
#include <expected>
#include <ranges>

#include "apps/app.hpp"
#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/delay.hpp"
#include "libs/mcu/i2c.hpp"

namespace app {
using std::chrono::operator""ms;

auto AppMain(board::Board& board) -> std::expected<void, common::Error> {
  I2CDemo i2c_demo{board};
  if (!i2c_demo.Init()) {
    return std::unexpected(common::Error::kUnknown);
  }
  if (!i2c_demo.Run()) {
    return std::unexpected(common::Error::kUnknown);
  }
  return {};
}

auto I2CDemo::Init() -> std::expected<void, common::Error> {
  return board_.Init();
}

auto I2CDemo::Run() -> std::expected<void, common::Error> {
  // I2C device address to test
  constexpr uint16_t kDeviceAddress{0x50};

  // Test pattern to write/read
  const std::array<uint8_t, 4> test_pattern{0xDE, 0xAD, 0xBE, 0xEF};

  // Main loop - write pattern, read it back, verify
  while (true) {
    // Write test pattern to I2C device
    auto write_result = board_.I2C1().SendData(kDeviceAddress, test_pattern);
    if (!write_result) {
      // Turn off LED1 on write error
      std::ignore = board_.UserLed1().SetLow();
      mcu::Delay(100ms);
      continue;
    }

    // Small delay between write and read
    mcu::Delay(50ms);

    // Read data back from I2C device
    auto read_result{
        board_.I2C1().ReceiveData(kDeviceAddress, test_pattern.size())};
    if (!read_result) {
      // Turn off LED1 on read error
      std::ignore = board_.UserLed1().SetLow();
      mcu::Delay(100ms);
      continue;
    }

    // Verify received data matches test pattern
    const auto received_span{read_result.value()};
    bool data_matches{true};
    if (std::ranges::equal(received_span, test_pattern)) {
      data_matches = false;
    }

    // Toggle LED1 based on verification result
    if (data_matches) {
      // Data matches - toggle LED1
      const auto led_state = board_.UserLed1().Get();
      if (led_state && led_state.value() == mcu::PinState::kHigh) {
        std::ignore = board_.UserLed1().SetLow();
      } else {
        std::ignore = board_.UserLed1().SetHigh();
      }
    } else {
      // Data mismatch - turn off LED1
      std::ignore = board_.UserLed1().SetLow();
    }

    // Toggle LED2 to show we're alive
    const auto led2_state = board_.UserLed2().Get();
    if (led2_state && led2_state.value() == mcu::PinState::kHigh) {
      std::ignore = board_.UserLed2().SetLow();
    } else {
      std::ignore = board_.UserLed2().SetHigh();
    }

    // Delay before next iteration
    mcu::Delay(500ms);
  }

  return {};
}

}  // namespace app
