#pragma once

#include <expected>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "libs/common/error.hpp"
#include "receiver.hpp"

namespace mcu {

using ReceiverMap = const std::vector<
    std::pair<std::function<bool(const std::string_view& message)>, Receiver&>>;

class Dispatcher {
 public:
  explicit Dispatcher(const ReceiverMap& receivers) : receivers_{receivers} {}

  ~Dispatcher() = default;
  Dispatcher(const Dispatcher&) = delete;
  Dispatcher(Dispatcher&&) = delete;
  auto operator=(const Dispatcher&) -> Dispatcher& = delete;
  auto operator=(Dispatcher&&) -> Dispatcher& = delete;

  auto Dispatch(const std::string_view& message) const
      -> std::expected<std::string, common::Error> {
    for (const auto& [predicate, receiver] : receivers_) {
      if (predicate(message)) {
        auto reply = receiver.Receive(message);
        if (reply.has_value()) {
          return reply;
        }
      }
    }
    return std::unexpected(common::Error::kUnhandled);
  }

 private:
  const ReceiverMap& receivers_;
};

}  // namespace mcu
