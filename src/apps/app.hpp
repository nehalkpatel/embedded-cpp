#pragma once

#include <expected>

#include "board.hpp"
#include "error.hpp"

namespace app {
  auto app_main(board::Board& board) -> std::expected<void, Error>;
}  // namespace app
