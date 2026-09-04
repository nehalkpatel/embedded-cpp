#include "libs/mcu/delay.hpp"

#include <chrono>
#include <cstdint>

#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/systick.hpp"

namespace mcu {

namespace {

// Sub-millisecond waits spin on the DWT cycle counter, which the SysTick
// interrupt is too coarse to serve. Enabled lazily: DWT lives in the debug
// block and is not required to be running after reset.
constexpr std::uint32_t kSystemCoreClockHz = 16'000'000;
constexpr std::uint32_t kCyclesPerMicrosecond = kSystemCoreClockHz / 1'000'000;

auto EnableCycleCounter() -> void {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

auto SpinCycles(std::uint32_t cycles) -> void {
  EnableCycleCounter();
  const std::uint32_t start = DWT->CYCCNT;
  // Unsigned subtraction, so the counter's 32-bit wrap needs no special case.
  while ((DWT->CYCCNT - start) < cycles) {
  }
}

}  // namespace

auto Delay(std::chrono::microseconds duration) -> void {
  if (duration.count() <= 0) {
    return;
  }

  const auto microseconds = static_cast<std::uint64_t>(duration.count());
  const auto whole_milliseconds =
      static_cast<std::uint32_t>(microseconds / 1'000);
  const auto remainder = static_cast<std::uint32_t>(microseconds % 1'000);

  if (whole_milliseconds > 0) {
    // Wait for whole_milliseconds *edges*, not elapsed time: entering this
    // function part-way through a tick would otherwise round the wait down.
    const std::uint32_t start = Millis();
    while ((Millis() - start) <= whole_milliseconds) {
    }
  }

  if (remainder > 0) {
    SpinCycles(remainder * kCyclesPerMicrosecond);
  }
}

}  // namespace mcu
