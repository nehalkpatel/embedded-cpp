#pragma once

#include <chrono>

namespace mcu {

/// @brief Block the calling thread for at least `duration`.
/// Callers pass any chrono duration (e.g. 200ms); it converts implicitly.
/// On the host this is sleep_for, so only the calling thread pauses — the
/// transport's server thread keeps handling emulator messages throughout.
auto Delay(std::chrono::microseconds duration) -> void;

}  // namespace mcu
