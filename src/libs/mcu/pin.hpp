#pragma once

#include <expected>

#include "libs/common/error.hpp"

namespace mcu {

enum class PinDirection { kInput = 1, kOutput };
enum class PinState { kLow = 1, kHigh, kHighZ };

class Pin {
 public:
  virtual ~Pin() = default;

  virtual auto Configure(PinDirection direction)
      -> std::expected<void, common::Error> = 0;
  virtual auto SetHigh() -> std::expected<void, common::Error> = 0;
  virtual auto SetLow() -> std::expected<void, common::Error> = 0;
  virtual auto Get() -> std::expected<PinState, common::Error> = 0;
};
}  // namespace mcu
