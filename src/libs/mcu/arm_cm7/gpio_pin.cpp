#include "libs/mcu/arm_cm7/gpio_pin.hpp"

#include <expected>
#include <functional>
#include <utility>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/exti.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/arm_cm7/gpio_registers.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

auto GpioPin::Configure(PinDirection direction)
    -> std::expected<void, common::Error> {
  // ConfigurePin enables the port clock first, which matters: before it is
  // running every register here reads as zero and ignores writes, silently.
  ConfigurePin(port_, pin_,
               {
                   .mode = direction == PinDirection::kOutput ? PinMode::kOutput
                                                              : PinMode::kInput,
                   .speed = PinSpeed::kLow,
                   .pull = PinPull::kNone,
               });

  direction_ = direction;
  configured_ = true;
  return {};
}

auto GpioPin::Get() -> std::expected<PinState, common::Error> {
  if (!configured_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  // IDR, not ODR, for both directions: it reports what the pad is actually at,
  // so a shorted or externally driven output reads as what it really is rather
  // than as what it was told to be.
  auto* registers = PortRegisters(port_);
  return (registers->IDR & Mask()) != 0U ? PinState::kHigh : PinState::kLow;
}

auto GpioPin::SetHigh() -> std::expected<void, common::Error> {
  if (!configured_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (direction_ != PinDirection::kOutput) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  // BSRR sets from its low half and resets from its high half, in one write.
  // Read-modify-writing ODR instead would lose a concurrent change from an
  // interrupt handler touching another pin on the same port.
  PortRegisters(port_)->BSRR = Mask();
  return {};
}

auto GpioPin::SetLow() -> std::expected<void, common::Error> {
  if (!configured_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (direction_ != PinDirection::kOutput) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  PortRegisters(port_)->BSRR = Mask() << 16U;
  return {};
}

auto GpioPin::Toggle() -> std::expected<void, common::Error> {
  if (!configured_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (direction_ != PinDirection::kOutput) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  // Read the current level from ODR (what was commanded, not what the pad
  // reads) and write the opposite through BSRR. The GPIO block has no
  // atomic toggle, so this is a read-modify-write of one bit -- but the write
  // half goes through BSRR, so it cannot disturb the port's other pins.
  auto* registers = PortRegisters(port_);
  const bool is_high = (registers->ODR & Mask()) != 0U;
  registers->BSRR = is_high ? (Mask() << 16U) : Mask();
  return {};
}

auto GpioPin::SetInterruptHandler(std::function<void()> handler,
                                  PinTransition transition)
    -> std::expected<void, common::Error> {
  if (!configured_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (direction_ != PinDirection::kInput) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  return RegisterExtiHandler(port_, pin_, std::move(handler), transition);
}

}  // namespace mcu
