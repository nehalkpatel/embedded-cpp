#pragma once

#include <expected>

#include "libs/board/board.hpp"
#include "libs/common/error.hpp"

namespace app {
auto AppMain(board::Board& board) -> std::expected<void, common::Error>;
}  // namespace app
