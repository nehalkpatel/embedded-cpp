#pragma once

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include "libs/common/error.hpp"
#include "receiver.hpp"

namespace mcu {

using ReceiverMap = std::vector<std::reference_wrapper<Receiver>>;

/// Offers each incoming message to the receivers in order; the first one to
/// accept it (see the Receiver contract) produces the reply. Receivers decide
/// for themselves whether a message is theirs — typically by decoding it and
/// checking the addressed peripheral name.
class Dispatcher {
 public:
  explicit Dispatcher(const ReceiverMap& receivers) : receivers_{receivers} {}

  ~Dispatcher() = default;
  Dispatcher(const Dispatcher&) = delete;
  Dispatcher(Dispatcher&&) = delete;
  auto operator=(const Dispatcher&) -> Dispatcher& = delete;
  auto operator=(Dispatcher&&) -> Dispatcher& = delete;

  [[nodiscard]] auto Dispatch(std::string_view message) const
      -> std::expected<std::string, common::Error> {
    for (const auto& receiver : receivers_) {
      if (auto reply = receiver.get().Receive(message); reply.has_value()) {
        return reply;
      }
    }
    return std::unexpected(common::Error::kUnhandled);
  }

 private:
  const ReceiverMap& receivers_;
};

}  // namespace mcu
