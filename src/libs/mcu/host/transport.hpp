#pragma once

#include <expected>
#include <string>

#include "libs/common/error.hpp"

namespace mcu {

/// Blocking, message-oriented channel to the emulator. Send transmits one
/// encoded message; Receive blocks for the reply to it.
class Transport {
 public:
  virtual ~Transport() = default;
  [[nodiscard]] virtual auto Send(std::string_view data)
      -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto Receive()
      -> std::expected<std::string, common::Error> = 0;
};

}  // namespace mcu
