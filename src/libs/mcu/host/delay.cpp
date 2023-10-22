#include "libs/mcu/delay.hpp"

#include <thread>

namespace mcu {
  auto delay(std::chrono::microseconds us) -> void {
    std::this_thread::sleep_for(us);
  }
}  // namespace mcu