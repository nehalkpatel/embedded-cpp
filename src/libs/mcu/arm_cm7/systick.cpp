#include "libs/mcu/arm_cm7/systick.hpp"

#include <cstdint>

#include "libs/mcu/arm_cm7/cmsis.hpp"

namespace mcu {

namespace {

// The reset clock configuration: HSI, 16 MHz, no PLL. The board deliberately
// does not raise this yet -- a PLL, the caches and the ART accelerator are all
// changes that can break working peripherals, and are worth making one at a
// time against a known-good baseline.
constexpr std::uint32_t kSystemCoreClockHz = 16'000'000;
constexpr std::uint32_t kTickRateHz = 1'000;

// Written by the interrupt handler, read by everything else.
volatile std::uint32_t g_ticks = 0;

}  // namespace

// Read-modify-write spelled out: ++ on a volatile-qualified operand is
// deprecated in C++20, because the standard does not say how many accesses it
// makes. One load and one store is what is wanted here, and what this says.
extern "C" auto SysTick_Handler() -> void { g_ticks = g_ticks + 1; }

auto Millis() -> std::uint32_t { return g_ticks; }

auto SysTickRunning() -> bool {
  return (SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) != 0U;
}

auto InitSysTick() -> void { SysTick_Config(kSystemCoreClockHz / kTickRateHz); }

}  // namespace mcu
