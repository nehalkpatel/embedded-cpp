#include <chrono>

#pragma once

namespace mcu {
  auto delay(std::chrono::microseconds us) -> void;

}  // namespace mcu