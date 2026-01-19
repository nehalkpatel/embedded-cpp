#pragma once

#include <cstdint>
#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {

struct Board {
  virtual ~Board() = default;

  [[nodiscard]] virtual auto Init() -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto UserLed1() -> mcu::OutputPin& = 0;
  [[nodiscard]] virtual auto UserLed2() -> mcu::OutputPin& = 0;
  [[nodiscard]] virtual auto UserButton1() -> mcu::InputPin& = 0;
  [[nodiscard]] virtual auto I2C1() -> mcu::I2CController& = 0;
  [[nodiscard]] virtual auto Uart1() -> mcu::Uart& = 0;
};
}  // namespace board
