#pragma once

#include "app.hpp"
#include "board.hpp"

namespace app {
struct Blinky : public App<Blinky> {
  Blinky(board::Board& board) : board_(board) {}
  auto Run() -> std::expected<void, int>;
  auto Init() -> std::expected<void, int>;

 private:
  board::Board& board_;
};
}  // namespace app
