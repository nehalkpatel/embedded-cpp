#include "i2c_demo.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>

#include "apps/app.hpp"
#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/delay.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"

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
  const std::array<std::byte, 4> test_pattern{std::byte{0xDE}, std::byte{0xAD},
                                              std::byte{0xBE}, std::byte{0xEF}};

  // Main loop - write pattern, read it back, verify
  while (true) {
    // Write test pattern to I2C device
    auto write_result{board_.I2C1().SendData(kDeviceAddress, test_pattern)};
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
    const bool data_matches{std::ranges::equal(received_span, test_pattern)};

    // Toggle LED1 based on verification result
    if (data_matches) {
      std::ignore = board_.UserLed1().Toggle();
    } else {
      // Data mismatch - turn off LED1
      std::ignore = board_.UserLed1().SetLow();
    }

    // Toggle LED2 to show we're alive
    std::ignore = board_.UserLed2().Toggle();

    // Delay before next iteration
    mcu::Delay(500ms);
  }

  return {};
}

}  // namespace app
