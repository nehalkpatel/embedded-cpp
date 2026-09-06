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
/// Constructing a pin configures it: the constructor enables the port clock and
/// programs the pin, so there is no window in which an unconfigured GpioPin
/// exists and no operation has to ask whether there is one.
///
/// That is safe even though a board holds its pins as namespace-scope members.
/// Reset_Handler copies .data, zeroes .bss and calls SystemInit before
/// __libc_init_array, RCC is live out of reset, and ConfigurePin enables its
/// own port clock before touching anything else. See the invariants on
/// board::NucleoF767ZiBoard before adding a peripheral that needs more.
///
/// The direction can still be changed at run time -- that is what makes this a
/// BidirectionalPin -- so operations that require a particular direction still
/// check for it. That check is about what the pin is right now, not about
/// whether anyone remembered to set it up.
class GpioPin final : public BidirectionalPin {
 public:
  GpioPin(GpioPort port, std::uint32_t pin, PinDirection direction);

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
  /// Program the pin for a direction. Shared by the constructor and Configure,
  /// which is why it returns void rather than the expected Configure owes its
  /// caller: there is nothing here that can fail.
  auto ApplyDirection(PinDirection direction) -> void;

  [[nodiscard]] auto Mask() const -> std::uint32_t { return 1U << pin_; }

  GpioPort port_;
  std::uint32_t pin_;
  PinDirection direction_;
};

}  // namespace mcu
