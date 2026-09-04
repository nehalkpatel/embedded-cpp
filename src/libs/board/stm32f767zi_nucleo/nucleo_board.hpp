#pragma once

#include <expected>

#include "libs/board/board.hpp"
#include "libs/board/stm32f767zi_nucleo/unimplemented_peripherals.hpp"
#include "libs/common/error.hpp"
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
class NucleoF767ZiBoard final : public Board {
 public:
  [[nodiscard]] auto Init() -> std::expected<void, common::Error> override;

  [[nodiscard]] auto UserLed1() -> mcu::OutputPin& override;
  [[nodiscard]] auto UserLed2() -> mcu::OutputPin& override;
  [[nodiscard]] auto UserButton1() -> mcu::InputPin& override;
  [[nodiscard]] auto I2C1() -> mcu::I2CController& override;
  [[nodiscard]] auto Uart1() -> mcu::Uart& override;

 private:
  // Placeholders until each peripheral's hardware implementation lands. See
  // unimplemented_peripherals.hpp.
  UnimplementedPin user_led_1_;
  UnimplementedPin user_led_2_;
  UnimplementedPin user_button_1_;
  UnimplementedI2CController i2c_1_;
  UnimplementedUart uart_1_;
};

}  // namespace board
