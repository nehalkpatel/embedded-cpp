#include <iostream>

#include "apps/app.hpp"
#include "libs/board/host/host_board.hpp"

auto main() -> int {
  try {
    board::HostBoard board{};

    if (!app::app_main(board)) {
      std::cout << "app_main failed" << '\n';
      exit(EXIT_FAILURE);
    }

  } catch (std::exception& exc) {
    std::cerr << exc.what() << '\n';
    exit(EXIT_FAILURE);
  }

  return (EXIT_SUCCESS);
}
