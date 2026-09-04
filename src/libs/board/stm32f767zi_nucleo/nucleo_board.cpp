#include "libs/board/stm32f767zi_nucleo/nucleo_board.hpp"

#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/systick.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {

auto NucleoF767ZiBoard::Init() -> std::expected<void, common::Error> {
  // The core is already configured: SystemInit ran from Reset_Handler, before
  // .data was copied. What is left is everything that needs a working C++
  // runtime -- starting with the tick that mcu::Delay is built on.
  mcu::InitSysTick();

  // The button is externally pulled down on this board (UM1974), so it needs
  // no internal pull: it reads low at rest and high while pressed.
  return user_led_1_.Configure(mcu::PinDirection::kOutput)
      .and_then(
          [this] { return user_led_2_.Configure(mcu::PinDirection::kOutput); })
      .and_then([this] {
        return user_button_1_.Configure(mcu::PinDirection::kInput);
      })
      // I2C has no Init() in the portable interface -- unlike Uart, which the
      // application configures itself -- so the board brings the bus up here.
      .and_then([this] { return i2c_1_.Init(); });
}

auto NucleoF767ZiBoard::UserLed1() -> mcu::OutputPin& { return user_led_1_; }
auto NucleoF767ZiBoard::UserLed2() -> mcu::OutputPin& { return user_led_2_; }
auto NucleoF767ZiBoard::UserButton1() -> mcu::InputPin& {
  return user_button_1_;
}
auto NucleoF767ZiBoard::I2C1() -> mcu::I2CController& { return i2c_1_; }
auto NucleoF767ZiBoard::Uart1() -> mcu::Uart& { return uart_1_; }

}  // namespace board
