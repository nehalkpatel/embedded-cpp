#pragma once

#include <expected>
#include <string>

#include "libs/common/error.hpp"

namespace mcu {

/// A component that can handle messages arriving from the emulator.
///
/// Contract: return the encoded reply when the message was handled; return an
/// unexpected error to mean "not mine" — the Dispatcher then keeps looking for
/// another receiver, and only reports kUnhandled if none accepts it.
class Receiver {
 public:
  virtual ~Receiver() = default;

  [[nodiscard]] virtual auto Receive(std::string_view message)
      -> std::expected<std::string, common::Error> = 0;
};

}  // namespace mcu
