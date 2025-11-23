#include "host_board.hpp"

#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {
auto HostBoard::Init() -> std::expected<void, common::Error> {
  {
    const auto res{user_led_1_.Configure(mcu::PinDirection::kOutput)};
    if (!res) {
      return res;
    }
  }

  {
    const auto res{user_led_2_.Configure(mcu::PinDirection::kOutput)};
    if (!res) {
      return res;
    }
  }

  {
    const auto res{user_button_1_.Configure(mcu::PinDirection::kInput)};
    if (!res) {
      return res;
    }
  }
  return {};
}
auto HostBoard::UserLed1() -> mcu::OutputPin& { return user_led_1_; }
auto HostBoard::UserLed2() -> mcu::OutputPin& { return user_led_2_; }
auto HostBoard::UserButton1() -> mcu::InputPin& { return user_button_1_; }
auto HostBoard::I2C1() -> mcu::I2CController& { return i2c1_; }
auto HostBoard::Uart1() -> mcu::Uart& { return uart_1_; }
}  // namespace board
