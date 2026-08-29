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
using std::chrono::operator""ms;

auto AppMain(board::Board& board) -> std::expected<void, common::Error> {
  return RunApp<Blinky>(board);
}

auto Blinky::Run() -> std::expected<void, common::Error> {
  if (auto status = board_.UserLed1().SetHigh(); !status) {
    return status;
  }
  while (true) {
    mcu::Delay(200ms);
    if (auto status = board_.UserLed1().Toggle(); !status) {
      return status;
    }
  }
}

auto Blinky::Init() -> std::expected<void, common::Error> {
  return board_.Init().and_then([this]() {
    return board_.UserButton1().SetInterruptHandler(
        [this]() { std::ignore = board_.UserLed2().SetHigh(); },
        mcu::PinTransition::kRising);
  });
}

}  // namespace app
