#pragma once

#include <cstdint>
#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"

namespace board {

struct Board {
  virtual ~Board() = default;

  virtual auto Init() -> std::expected<void, common::Error> = 0;
  virtual auto UserLed1() -> mcu::OutputPin& = 0;
  virtual auto UserLed2() -> mcu::OutputPin& = 0;
  virtual auto UserButton1() -> mcu::InputPin& = 0;
  virtual auto I2C1() -> mcu::I2CController& = 0;
};
}  // namespace board
