#pragma once

#include <expected>

#include "libs/board/board.hpp"
#include "libs/common/error.hpp"

namespace app {
auto app_main(board::Board& board) -> std::expected<void, Error>;
}  // namespace app
