#pragma once

#include <expected>

#include "libs/common/error.hpp"

namespace mcu {

enum class PinDirection { kInput, kOutput };
enum class PinState { kLow, kHigh, kHighZ };

struct Pin {
  virtual ~Pin() = default;

  virtual auto Configure(PinDirection direction) -> std::expected<void, Error> = 0;
  virtual auto SetHigh() -> std::expected<void, Error> = 0;
  virtual auto SetLow() -> std::expected<void, Error> = 0;
  virtual auto Get() -> std::expected<PinState, Error> = 0;
};

}  // namespace mcu
