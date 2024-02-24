#pragma once

#include <expected>
#include <thread>
#include <zmq.hpp>

#include "libs/common/error.hpp"
#include "message_dispatcher.hpp"
#include "transport.hpp"

namespace mcu {
class ZmqTransport : public Transport {
 public:
  ZmqTransport(const std::string& to_emulator, const std::string& from_emulator,
               MessageDispatcher& dispatcher);
  ZmqTransport(const ZmqTransport&) = delete;
  ZmqTransport(ZmqTransport&&) = delete;
  auto operator=(const ZmqTransport&) -> ZmqTransport& = delete;
  auto operator=(ZmqTransport&&) -> ZmqTransport& = delete;
  ~ZmqTransport() override;

  auto Send(std::string_view data)
      -> std::expected<void, common::Error> override;
  auto Receive() -> std::expected<std::string, common::Error> override;

 private:
  auto ServerThread(const std::string& endpoint) -> void;

  zmq::context_t to_emulator_context_{1};
  zmq::socket_t to_emulator_socket_{to_emulator_context_,
                                    zmq::socket_type::pair};
  zmq::context_t from_emulator_context_{1};

  std::atomic<bool> running_{true};
  MessageDispatcher& dispatcher_;
  std::thread server_thread_;
};

}  // namespace mcu
