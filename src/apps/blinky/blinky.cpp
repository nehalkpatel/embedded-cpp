#include "blinky.hpp"

#include <expected>

#include "app.hpp"
#include "board.hpp"
#include "error.hpp"
#include "pin.hpp"

namespace app {

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
  auto status = board_.UserLed1().SetHigh();
  if (!status) {
    return std::unexpected(status.error());
  }
  return {};
}

auto Blinky::Init() -> std::expected<void, Error> { return board_.Init(); }

}  // namespace app
