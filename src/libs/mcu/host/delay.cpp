#include "libs/mcu/delay.hpp"

#include <chrono>
#include <thread>

namespace mcu {
auto Delay(std::chrono::microseconds duration) -> void {
  std::this_thread::sleep_for(duration);
}
}  // namespace mcu
