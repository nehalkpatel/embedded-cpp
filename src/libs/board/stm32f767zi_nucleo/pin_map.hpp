#pragma once

#include <cstdint>

#include "libs/mcu/arm_cm7/cmsis.hpp"

/// @file
/// Every board-specific pin assignment, in one table.
///
/// Source: UM1974, "STM32 Nucleo-144 boards". The user LEDs and button are on
/// the board itself; the I2C and USART assignments follow the Arduino and
/// ST-LINK virtual COM port wiring respectively.
namespace board::pin_map {

struct PinLocation {
  GPIO_TypeDef* port;
  std::uint32_t pin;  ///< Bit position within the port, 0-15.
};

// LD1 green, LD2 blue, LD3 red. LD3 is not exposed through board::Board yet.
constexpr PinLocation kUserLed1{GPIOB, 0};
constexpr PinLocation kUserLed2{GPIOB, 7};
constexpr PinLocation kUserLed3{GPIOB, 14};

// B1, the blue user button. Externally pulled down, so it reads high when
// pressed and needs no internal pull.
constexpr PinLocation kUserButton1{GPIOC, 13};

// USART3 is wired to the ST-LINK virtual COM port: no external adapter needed,
// output shows up on /dev/ttyACM0. Alternate function 7.
constexpr PinLocation kUart1Tx{GPIOD, 8};
constexpr PinLocation kUart1Rx{GPIOD, 9};
constexpr std::uint32_t kUart1AlternateFunction = 7;

// I2C1 on the Arduino connector: D15 (SCL) and D14 (SDA). Alternate function 4.
constexpr PinLocation kI2C1Scl{GPIOB, 8};
constexpr PinLocation kI2C1Sda{GPIOB, 9};
constexpr std::uint32_t kI2C1AlternateFunction = 4;

}  // namespace board::pin_map
