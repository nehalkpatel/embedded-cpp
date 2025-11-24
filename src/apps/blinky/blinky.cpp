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
    status = status
                 .and_then([this]() {
                   mcu::Delay(500ms);
                   return board_.UserLed1().Get();
                 })
                 .and_then([this](mcu::PinState state) {
                   return (state == mcu::PinState::kHigh)
                              ? board_.UserLed1().SetLow()
                              : board_.UserLed1().SetHigh();
                 });

    if (!status) {
      return std::unexpected(status.error());
    }
  }
  return {};
}

auto Blinky::Init() -> std::expected<void, common::Error> {
  return board_.Init().and_then([this]() {
    return board_.UserButton1().SetInterruptHandler(
        [this]() { std::ignore = board_.UserLed2().SetHigh(); },
        mcu::PinTransition::kRising);
  });
}

}  // namespace app
