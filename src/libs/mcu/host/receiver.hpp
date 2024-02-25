#pragma once

#include <expected>
#include <string>

#include "libs/common/error.hpp"

namespace mcu {

class Receiver {
 public:
  virtual auto Receive(const std::string_view& message)
      -> std::expected<std::string, common::Error> = 0;
};

}  // namespace mcu
