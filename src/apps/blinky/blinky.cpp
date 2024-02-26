#include "blinky.hpp"

#include <chrono>
#include <expected>
#include <functional>

#include "apps/app.hpp"
#include "libs/board/board.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/delay.hpp"
#include "libs/mcu/pin.hpp"

namespace app {
using namespace std::chrono_literals;

auto AppMain(board::Board& board) -> std::expected<void, common::Error> {
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
    mcu::Delay(500ms);
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
  auto status = board_.Init();
  if (status) {
    return board_.UserButton1().SetInterruptHandler(
        []() { return; }, mcu::PinTransition::kRising);
  }
  return status;
}

}  // namespace app
