#pragma once

#include "libs/board/board.hpp"

namespace app {

class I2CDemo {
 public:
  explicit I2CDemo(board::Board& board) : board_(board) {}
  auto Init() -> std::expected<void, common::Error>;
  auto Run() -> std::expected<void, common::Error>;

 private:
  board::Board& board_;
};

}  // namespace app
