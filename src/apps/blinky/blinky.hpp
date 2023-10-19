#pragma once

#include "apps/app.hpp"
#include "libs/board/board.hpp"

namespace app {

class Blinky {
 public:
  Blinky(board::Board& board) : board_(board) {}
  auto Init() -> std::expected<void, Error>;
  auto Run() -> std::expected<void, Error>;

 private:
  board::Board& board_;
};

}  // namespace app