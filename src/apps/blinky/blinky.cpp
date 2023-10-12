#include "blinky.hpp"

namespace app{

  auto Blinky::Run() -> std::expected<void, int> { (void) board_; return {}; }
  auto Blinky::Init() -> std::expected<void, int> { return {}; }

} // namespace app
