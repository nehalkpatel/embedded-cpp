#pragma once

#include <cstdint>
#include <expected>
#include <functional>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

/// @brief Register a handler for edges on one EXTI line.
///
/// The STM32 routes external interrupts by pin *number*, not by port: line 3
/// is PA3, PB3, PC3... and only one of them at a time. Registering a second
/// port on a line already claimed by another returns kInvalidState rather
/// than silently rerouting the first one away.
///
/// The handler runs in interrupt context. It is stored, so it must own
/// whatever it captures -- see the note on std::function in libs/mcu/pin.hpp.
[[nodiscard]] auto RegisterExtiHandler(
    GpioPort port, std::uint32_t pin, std::function<void()> handler,
    PinTransition transition) -> std::expected<void, common::Error>;

}  // namespace mcu
