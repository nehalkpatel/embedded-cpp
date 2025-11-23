#pragma once

#include "libs/board/board.hpp"

namespace app {

class UartEcho {
 public:
  explicit UartEcho(board::Board& board) : board_(board) {}
  auto Init() -> std::expected<void, common::Error>;
  auto Run() -> std::expected<void, common::Error>;

 private:
  board::Board& board_;
};

}  // namespace app
