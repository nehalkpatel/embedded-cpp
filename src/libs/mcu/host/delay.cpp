#include "libs/mcu/delay.hpp"

#include <chrono>
#include <thread>

namespace mcu {
auto Delay(std::chrono::microseconds usecs) -> void {
  std::this_thread::sleep_for(usecs);
}
}  // namespace mcu
