#pragma once

#include <cstdint>

namespace mcu {

/// @brief Ticks since Init(), one per millisecond.
///
/// Wraps after ~49 days. Compare differences rather than absolute values and
/// unsigned arithmetic makes the wrap harmless.
[[nodiscard]] auto Millis() -> std::uint32_t;

/// @brief Start the 1 kHz system tick.
///
/// Idempotent. Called from the board's Init() rather than from SystemInit,
/// because it enables an interrupt and must not run before .bss is zeroed.
auto InitSysTick() -> void;

}  // namespace mcu
