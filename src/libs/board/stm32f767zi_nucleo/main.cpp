#include <utility>

#include "apps/app.hpp"
#include "libs/board/stm32f767zi_nucleo/nucleo_board.hpp"

namespace {

// Namespace scope, not a local in main(), for two reasons. The board owns
// peripheral state that interrupt handlers reach for, and an ISR can fire
// before main() has entered its loop. And its construction here is what
// exercises .init_array: Reset_Handler calls __libc_init_array before main,
// and if the linker script's KEEP on that section were ever dropped, this
// object would silently never be constructed.
board::NucleoF767ZiBoard g_board;

}  // namespace

extern "C" auto main() -> int {
  if (auto result = app::AppMain(g_board); !result) {
    // Nowhere to report to yet: this board has no console until USART3 is
    // implemented. Keep the error where a debugger can read it and stop, so a
    // failure is a halt at a known place rather than a silently idle board.
    const auto error = std::to_underlying(result.error());
    static_cast<void>(error);
    while (true) {
    }
  }

  // main() must not return on bare metal. Reset_Handler hangs if it does; be
  // explicit here rather than relying on that.
  while (true) {
  }
}
