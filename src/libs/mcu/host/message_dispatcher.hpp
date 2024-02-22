#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "message_receiver.hpp"

namespace mcu {

using ReceiverMap =
    std::vector<std::pair<std::function<bool(const std::string_view& message)>,
                          MessageReceiver&>>;

class MessageDispatcher {
 public:
  explicit MessageDispatcher(ReceiverMap& receivers)
      : receivers_{std::move(receivers)} {}

  ~MessageDispatcher() = default;
  MessageDispatcher(const MessageDispatcher&) = delete;
  MessageDispatcher(MessageDispatcher&&) = delete;
  auto operator=(const MessageDispatcher&) -> MessageDispatcher& = delete;
  auto operator=(MessageDispatcher&&) -> MessageDispatcher& = delete;

  void DispatchMessage(const std::string_view& message) {
    for (const auto& [predicate, receiver] : receivers_) {
      if (predicate(message)) {
        receiver.Receive(message);
        return;
      }
    }
  }

 private:
  const ReceiverMap& receivers_;
};

}  // namespace mcu
