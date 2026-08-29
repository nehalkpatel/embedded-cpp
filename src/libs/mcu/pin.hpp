#pragma once

#include <cstdint>
#include <expected>
#include <functional>

#include "libs/common/error.hpp"

namespace mcu {

enum class PinDirection : std::uint8_t { kInput = 1, kOutput };
enum class PinState : std::uint8_t { kLow = 1, kHigh, kHighZ };
enum class PinTransition : std::uint8_t { kRising = 1, kFalling, kBoth };

class InputPin {
 public:
  virtual ~InputPin() = default;
  [[nodiscard]] virtual auto Get()
      -> std::expected<PinState, common::Error> = 0;
  [[nodiscard]] virtual auto SetInterruptHandler(std::function<void()> handler,
                                                 PinTransition transition)
      -> std::expected<void, common::Error> = 0;
};

class OutputPin : public virtual InputPin {
 public:
  ~OutputPin() override = default;

  [[nodiscard]] virtual auto SetHigh()
      -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto SetLow() -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto Toggle() -> std::expected<void, common::Error> = 0;
};

class BidirectionalPin : public virtual InputPin, public virtual OutputPin {
 public:
  ~BidirectionalPin() override = default;

  [[nodiscard]] virtual auto Configure(PinDirection direction)
      -> std::expected<void, common::Error> = 0;
};
}  // namespace mcu
