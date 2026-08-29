#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>
#include <utility>

#include "apps/app.hpp"
#include "libs/board/host/host_board.hpp"

auto main() -> int {
  // Project code is exception-free (std::expected throughout), but the host
  // build links libraries that can throw (cppzmq, nlohmann-json). main is the
  // boundary that converts an escaped exception into a failing exit code —
  // via return, not exit(), so the board's destructor still runs its
  // transport shutdown.
  try {
    board::HostBoard board{};
    if (auto result = app::AppMain(board); !result) {
      std::println(stderr, "AppMain failed (error {})",
                   std::to_underlying(result.error()));
      return EXIT_FAILURE;
    }
  } catch (const std::exception& exc) {
    // C stdio in the handlers: the one thing an exception handler must not
    // do is throw.
    static_cast<void>(
        std::fprintf(stderr, "Unhandled exception: %s\n", exc.what()));
    return EXIT_FAILURE;
  } catch (...) {
    static_cast<void>(std::fputs("Unhandled exception\n", stderr));
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
