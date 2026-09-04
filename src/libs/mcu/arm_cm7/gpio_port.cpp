#include "libs/mcu/arm_cm7/gpio_port.hpp"

#include <cstdint>

#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/gpio_registers.hpp"

namespace mcu {

auto PortRegisters(GpioPort port) -> GPIO_TypeDef* {
  // The port register blocks are contiguous at 0x400 intervals from GPIOA,
  // in the same order as the enumerators.
  const auto base = GPIOA_BASE + (static_cast<std::uintptr_t>(port) * 0x400UL);
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  return reinterpret_cast<GPIO_TypeDef*>(base);
}

auto EnablePortClock(GpioPort port) -> void {
  RCC->AHB1ENR |= 1UL << static_cast<std::uint32_t>(port);

  // A peripheral clock enable takes a couple of cycles to take effect. Reading
  // the register back stalls until the write has landed; without this, an
  // immediately following register write can be dropped.
  static_cast<void>(RCC->AHB1ENR);
}

}  // namespace mcu
