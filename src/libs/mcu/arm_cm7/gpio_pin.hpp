#pragma once

#include <cstdint>
#include <expected>
#include <functional>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

/// @brief One STM32 GPIO pin.
///
/// Construction only records where the pin is; it touches no registers, so a
/// board can hold pins as members and have them constructed before the clock
/// tree is up. Configure() is what makes the pin real, and every operation
/// before it returns kInvalidState rather than writing into a dead register
/// block.
///
/// The split is deliberate; that nothing enforces the second half of it is
/// not. Every peripheral in this backend has the same shape. See issue #37.
class GpioPin final : public BidirectionalPin {
 public:
  GpioPin(GpioPort port, std::uint32_t pin) : port_(port), pin_(pin) {}

  [[nodiscard]] auto Configure(PinDirection direction)
      -> std::expected<void, common::Error> override;

  [[nodiscard]] auto Get() -> std::expected<PinState, common::Error> override;
  [[nodiscard]] auto SetHigh() -> std::expected<void, common::Error> override;
  [[nodiscard]] auto SetLow() -> std::expected<void, common::Error> override;
  [[nodiscard]] auto Toggle() -> std::expected<void, common::Error> override;

  [[nodiscard]] auto SetInterruptHandler(std::function<void()> handler,
                                         PinTransition transition)
      -> std::expected<void, common::Error> override;

 private:
  [[nodiscard]] auto Mask() const -> std::uint32_t { return 1U << pin_; }

  GpioPort port_;
  std::uint32_t pin_;
  PinDirection direction_ = PinDirection::kInput;
  bool configured_ = false;
};

}  // namespace mcu
