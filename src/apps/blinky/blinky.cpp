#include "blinky.hpp"

#include <chrono>
#include <expected>

#include "apps/app.hpp"
#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/delay.hpp"
#include "libs/mcu/pin.hpp"

namespace app {
using namespace std::chrono_literals;

auto app_main(board::Board& board) -> std::expected<void, common::Error> {
  Blinky blinky{board};
  if (!blinky.Init()) {
    return std::unexpected(common::Error::kUnknown);
  }
  if (!blinky.Run()) {
    return std::unexpected(common::Error::kUnknown);
  }
  return {};
}

auto Blinky::Run() -> std::expected<void, common::Error> {
  auto status = board_.UserLed1().SetHigh();
  while (true) {
    if (!status) {
      return std::unexpected(status.error());
    }
    mcu::delay(500ms);
    auto state = board_.UserLed1().Get();
    if (!state) {
      return std::unexpected(state.error());
    }
    if (state.value() == mcu::PinState::kHigh) {
      status = board_.UserLed1().SetLow();
    } else {
      status = board_.UserLed1().SetHigh();
    }
  }
  return {};
}

auto Blinky::Init() -> std::expected<void, common::Error> {
  return board_.Init();
}

}  // namespace app
