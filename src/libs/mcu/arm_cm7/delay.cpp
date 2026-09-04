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
constexpr std::uint64_t kMaxSpinCycles = 0xFFFF'0000ULL;

auto EnableCycleCounter() -> void {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/// Busy-wait for a cycle count. Caps at just under a full wrap of the 32-bit
/// counter (~268 s at 16 MHz): a longer request cannot be distinguished from a
/// shorter one once the counter has lapped, so clamp rather than return early.
auto SpinCycles(std::uint64_t cycles) -> void {
  EnableCycleCounter();
  const auto bounded = static_cast<std::uint32_t>(
      cycles < kMaxSpinCycles ? cycles : kMaxSpinCycles);
  const std::uint32_t start = DWT->CYCCNT;
  // Unsigned subtraction, so the counter's 32-bit wrap needs no special case.
  while ((DWT->CYCCNT - start) < bounded) {
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

  // Before the board's Init() has started the tick, Millis() never advances
  // and waiting on it would never return. Spin on the cycle counter instead,
  // so an early Delay() is merely imprecise rather than a hang.
  if (!SysTickRunning()) {
    SpinCycles(microseconds * kCyclesPerMicrosecond);
    return;
  }

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
