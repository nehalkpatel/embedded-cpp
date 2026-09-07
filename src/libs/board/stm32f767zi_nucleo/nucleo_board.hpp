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
/// Constructing a peripheral configures it, which is what makes this list a
/// description of the hardware rather than a set of promises Init() has to
/// keep. Reset_Handler copies .data, zeroes .bss and calls SystemInit before
/// __libc_init_array, and RCC is live out of reset, so a peripheral constructor
/// can bring its own clock up. Three invariants hold that up, and a new
/// peripheral has to satisfy all three before it can be a member here:
///
///   1. Bring-up must not fail. A constructor cannot report an error. If a
///      peripheral has to poll a status bit that can time out, it needs a
///      separate call -- see mcu::Usart, which stays two-phase for this reason
///      as well as because only the application knows its UartConfig.
///   2. Bring-up must not talk to anything across a wire. mcu::Delay is fine
///      -- before the tick it falls back to a cycle-counter spin -- but a
///      *timeout* is not: Millis() is frozen until Init() starts the tick, so
///      a bounded wait cannot end and the drivers refuse one this early. That
///      makes off-chip bring-up Init()'s job. See docs/BOOT_FLOW.md.
///   3. This board must stay the only object with a dynamic initializer.
///      Construction order within it is declaration order and well defined;
///      order across translation units is not. `.init_array` holding one entry
///      besides crtbegin's is what that looks like in the map file.
class NucleoF767ZiBoard final : public Board {
 public:
  [[nodiscard]] auto Init() -> std::expected<void, common::Error> override;

  [[nodiscard]] auto UserLed1() -> mcu::OutputPin& override;
  [[nodiscard]] auto UserLed2() -> mcu::OutputPin& override;
  [[nodiscard]] auto UserButton1() -> mcu::InputPin& override;
  [[nodiscard]] auto I2C1() -> mcu::I2CController& override;
  [[nodiscard]] auto Uart1() -> mcu::Uart& override;

 private:
  mcu::GpioPin user_led_1_{pin_map::kUserLed1.port, pin_map::kUserLed1.pin,
                           mcu::PinDirection::kOutput};
  mcu::GpioPin user_led_2_{pin_map::kUserLed2.port, pin_map::kUserLed2.pin,
                           mcu::PinDirection::kOutput};

  // The button is externally pulled down on this board (UM1974), so it needs
  // no internal pull: it reads low at rest and high while pressed.
  mcu::GpioPin user_button_1_{pin_map::kUserButton1.port,
                              pin_map::kUserButton1.pin,
                              mcu::PinDirection::kInput};

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
                     },
                     // The driver brings the bus up on the pins' internal
                     // pull-ups, which it notes are weak (~40k) and good for
                     // 100 kHz over short wiring. Going faster is a decision
                     // for whoever adds external resistors.
                     mcu::I2CSpeed::kStandard100kHz};
};

}  // namespace board
