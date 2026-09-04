#pragma once

#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"

namespace mcu {

/// @brief The register block for a port.
///
/// Deliberately not in gpio_port.hpp: its return type is a vendor type, so
/// this header may only be included from .cpp files. See the note on macro
/// collisions in gpio_port.hpp.
[[nodiscard]] auto PortRegisters(GpioPort port) -> GPIO_TypeDef*;

}  // namespace mcu
