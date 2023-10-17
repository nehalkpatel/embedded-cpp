#pragma once

#include <cstdint>
#include <expected>

#include "error.hpp"
#include "i2c.hpp"
#include "pin.hpp"

namespace board {

struct Board {
  virtual ~Board() = default;

  virtual auto Init() -> std::expected<void, Error> = 0;
  virtual auto UserLed1() -> mcu::Pin& = 0;
  virtual auto UserButton1() -> mcu::Pin& = 0;
  virtual auto I2C1() -> mcu::I2CController& = 0;
};
}  // namespace board
