#pragma once

#include <expected>

#include "libs/board/board.hpp"
#include "libs/common/error.hpp"

namespace app {

/// Implemented once per application; the platform entry point (main) calls it
/// with the concrete board.
auto AppMain(board::Board& board) -> std::expected<void, common::Error>;

/// Construct, initialize, and run an application, propagating the first
/// error. Every AppMain is a one-line call to this.
template <typename App>
auto RunApp(board::Board& board) -> std::expected<void, common::Error> {
  App application{board};
  return application.Init().and_then(
      [&application] { return application.Run(); });
}

}  // namespace app
