#include <chrono>

#pragma once

namespace mcu {
auto Delay(std::chrono::microseconds usecs) -> void;

}  // namespace mcu
