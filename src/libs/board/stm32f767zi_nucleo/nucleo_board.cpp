#include "libs/board/stm32f767zi_nucleo/nucleo_board.hpp"

#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/systick.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {

auto NucleoF767ZiBoard::Init() -> std::expected<void, common::Error> {
  // The core is already configured: SystemInit ran from Reset_Handler, and the
  // pins and the I2C bus configured themselves as this object was constructed.
  // What is left is everything that cannot happen before main() -- which is
  // just the tick that mcu::Delay is built on, since it needs the NVIC.
  //
  // The USART is absent on purpose: mcu::Uart::Init is the application's to
  // call, because only the application knows the UartConfig it wants.
  mcu::InitSysTick();
  return {};
}

auto NucleoF767ZiBoard::UserLed1() -> mcu::OutputPin& { return user_led_1_; }
auto NucleoF767ZiBoard::UserLed2() -> mcu::OutputPin& { return user_led_2_; }
auto NucleoF767ZiBoard::UserButton1() -> mcu::InputPin& {
  return user_button_1_;
}
auto NucleoF767ZiBoard::I2C1() -> mcu::I2CController& { return i2c_1_; }
auto NucleoF767ZiBoard::Uart1() -> mcu::Uart& { return uart_1_; }

}  // namespace board
