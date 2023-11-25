#include "zmq_transport.hpp"

#include <expected>
#include <iostream>
#include <span>
#include <zmq.hpp>

#include "transport.hpp"

namespace mcu {
ZmqTransport::ZmqTransport(const std::string& endpoint) {
  socket_.connect(endpoint.c_str());
  if (socket_.handle() == nullptr) {
    throw std::runtime_error("Failed to connect to endpoint");
  }
}

auto ZmqTransport::Send(std::string_view data)
    -> std::expected<void, common::Error> {
  if (!socket_.send(zmq::buffer(data), zmq::send_flags::dontwait)) {
    return std::unexpected(common::Error::kOperationFailed);
  }
  return {};
}

auto ZmqTransport::Receive() -> std::expected<std::string, common::Error> {
  zmq::message_t msg;
  std::cout.flush();
  if (socket_.recv(msg, zmq::recv_flags::none) != msg.size()) {
    return std::unexpected(common::Error::kOperationFailed);
  }
  return msg.to_string();
}

}  // namespace mcu