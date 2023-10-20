#include "blinky.hpp"

#include <chrono>
#include <expected>
#include <thread>

#include "apps/app.hpp"
#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/pin.hpp"

namespace app {
using namespace std::chrono_literals;

auto app_main(board::Board& board) -> std::expected<void, Error> {
  Blinky blinky{board};
  if (!blinky.Init()) {
    return std::unexpected(Error::kUnknown);
  }
  if (!blinky.Run()) {
    return std::unexpected(Error::kUnknown);
  }
  return {};
}

auto Blinky::Run() -> std::expected<void, Error> {
  auto state = mcu::PinState::kHigh;
  auto status = board_.UserLed1().SetHigh();
  while (1) {
    if (!status) {
      return std::unexpected(status.error());
    }
    std::this_thread::sleep_for(500ms);
    if (state == mcu::PinState::kHigh) {
      state = mcu::PinState::kLow;
      status = board_.UserLed1().SetLow();
    } else {
      state = mcu::PinState::kHigh;
      status = board_.UserLed1().SetHigh();
    }
  }
  return {};
}

auto Blinky::Init() -> std::expected<void, Error> { return board_.Init(); }

}  // namespace app
