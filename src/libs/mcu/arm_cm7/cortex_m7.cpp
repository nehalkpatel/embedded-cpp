#include "libs/mcu/arm_cm7/cortex_m7.hpp"

#include "libs/mcu/arm_cm7/cmsis.hpp"

namespace mcu {

namespace {

// CPACR bits 20-23 are the CP10/CP11 access fields; 0b11 in each grants full
// access. CP10 and CP11 must always be programmed identically (ARMv7-M ARM).
constexpr auto kCp10FullAccess = 3U << 20U;
constexpr auto kCp11FullAccess = 3U << 22U;

}  // namespace

extern "C" auto SystemInit() -> void {
  SCB->CPACR |= kCp10FullAccess | kCp11FullAccess;

  // The vector table lives at the start of flash, where the boot ROM found it.
  // Set VTOR explicitly rather than relying on its reset value: it survives a
  // soft reset, and a bootloader may have moved it.
  SCB->VTOR = FLASH_BASE;
}

}  // namespace mcu
