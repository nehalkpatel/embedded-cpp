#pragma once

#include <expected>
#include <zmq.hpp>

#include "libs/common/error.hpp"
#include "transport.hpp"

namespace mcu {
class ZmqTransport : public Transport {
 public:
  explicit ZmqTransport(const std::string& endpoint);
  ZmqTransport(const ZmqTransport&) = delete;
  ZmqTransport(ZmqTransport&&) = delete;
  auto operator=(const ZmqTransport&) -> ZmqTransport& = delete;
  auto operator=(ZmqTransport&&) -> ZmqTransport& = delete;
  ~ZmqTransport() override = default;

  auto Send(std::string_view data)
      -> std::expected<void, common::Error> override;
  auto Receive() -> std::expected<std::string, common::Error> override;

 private:
  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, zmq::socket_type::pair};
};
}  // namespace mcu