#pragma once

#include <expected>

#include "libs/board/board.hpp"
#include "libs/board/stm32f767zi_nucleo/pin_map.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/gpio_pin.hpp"
#include "libs/mcu/arm_cm7/i2c.hpp"
#include "libs/mcu/arm_cm7/usart.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {

/// @brief The STM32F767ZI Nucleo-144 board.
///
/// Peripherals are members rather than pointers: there is no allocator worth
/// using here, and their lifetime is the board's. The board itself is a
/// namespace-scope object in main.cpp, so its constructor runs from
/// __libc_init_array before main().
///
/// Constructing a GpioPin touches no registers, which is what makes that safe
/// -- the pins record where they are, and Init() is what configures them.
class NucleoF767ZiBoard final : public Board {
 public:
  [[nodiscard]] auto Init() -> std::expected<void, common::Error> override;

  [[nodiscard]] auto UserLed1() -> mcu::OutputPin& override;
  [[nodiscard]] auto UserLed2() -> mcu::OutputPin& override;
  [[nodiscard]] auto UserButton1() -> mcu::InputPin& override;
  [[nodiscard]] auto I2C1() -> mcu::I2CController& override;
  [[nodiscard]] auto Uart1() -> mcu::Uart& override;

 private:
  mcu::GpioPin user_led_1_{pin_map::kUserLed1.port, pin_map::kUserLed1.pin};
  mcu::GpioPin user_led_2_{pin_map::kUserLed2.port, pin_map::kUserLed2.pin};
  mcu::GpioPin user_button_1_{pin_map::kUserButton1.port,
                              pin_map::kUserButton1.pin};

  mcu::Usart uart_1_{mcu::UsartId::kUsart3,
                     {
                         .tx_port = pin_map::kUart1Tx.port,
                         .tx_pin = pin_map::kUart1Tx.pin,
                         .rx_port = pin_map::kUart1Rx.port,
                         .rx_pin = pin_map::kUart1Rx.pin,
                         .alternate_function = pin_map::kUart1AlternateFunction,
                     }};

  mcu::I2CBus i2c_1_{mcu::I2CId::kI2C1,
                     {
                         .scl_port = pin_map::kI2C1Scl.port,
                         .scl_pin = pin_map::kI2C1Scl.pin,
                         .sda_port = pin_map::kI2C1Sda.port,
                         .sda_pin = pin_map::kI2C1Sda.pin,
                         .alternate_function = pin_map::kI2C1AlternateFunction,
                     }};
};

}  // namespace board
