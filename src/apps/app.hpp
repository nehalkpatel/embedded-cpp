#pragma once

#include <expected>

#include "board.hpp"

namespace app {

template <typename T>
  struct App {
    std::expected<void, int> Run() {
      return static_cast<T*>(this)->Run();
    }
    std::expected<void, int> Init() {
      return static_cast<T*>(this)->Init();
    }
  };

} // namespace app
