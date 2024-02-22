#pragma once

#include <string>

namespace mcu {

class MessageReceiver {
 public:
  virtual void Receive(const std::string_view& message) = 0;
};

}  // namespace mcu
