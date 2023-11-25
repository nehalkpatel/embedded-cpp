#pragma once

#include <expected>
#include <span>

#include "libs/common/error.hpp"

namespace mcu {
class Transport {
 public:
  virtual ~Transport() = default;
  virtual auto Send(std::string_view data)
      -> std::expected<void, common::Error> = 0;
  virtual auto Receive() -> std::expected<std::string, common::Error> = 0;

 private:
};
}  // namespace mcu