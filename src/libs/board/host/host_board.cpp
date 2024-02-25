#include "host_board.hpp"

#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"

namespace board {
auto HostBoard::Init() -> std::expected<void, common::Error> {
  auto res = user_led_1_.Configure(mcu::PinDirection::kOutput);
  if (res) {
    return user_button_1_.Configure(mcu::PinDirection::kInput);
  }
  return std::unexpected(common::Error::kUnknown);
}
auto HostBoard::UserLed1() -> mcu::OutputPin& { return user_led_1_; }
auto HostBoard::UserButton1() -> mcu::InputPin& { return user_button_1_; }
auto HostBoard::I2C1() -> mcu::I2CController& { return i2c1_; }
}  // namespace board
