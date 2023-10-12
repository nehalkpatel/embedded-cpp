#pragma once

#include <expected>

#include "error.hpp"

namespace mcu {

enum class PinDirection { kInput, kOutput };
enum class PinState { kLow, kHigh, kHighZ };

struct Pin {
  Pin() = default;
  virtual ~Pin() = default;
  Pin(const Pin&) = delete;
  Pin(Pin&&) = delete;
  auto operator=(const Pin&) -> Pin& = delete;
  auto operator=(Pin&&) -> Pin& = delete;

  virtual auto Configure(PinDirection direction) -> std::expected<void, Error> = 0;
  virtual auto SetHigh() -> std::expected<void, Error> = 0;
  virtual auto SetLow() -> std::expected<void, Error> = 0;
  virtual auto Get() -> std::expected<PinState, Error> = 0;
};

}  // namespace mcu
