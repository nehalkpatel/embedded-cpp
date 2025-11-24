#include "zmq_transport.hpp"

#include <expected>
#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "libs/common/error.hpp"

namespace mcu {
// NOLINTNEXTLINE
ZmqTransport::ZmqTransport(const std::string& to_emulator,
                           const std::string& from_emulator,
                           Dispatcher& dispatcher)
    : dispatcher_{dispatcher} {
  to_emulator_socket_.connect(to_emulator.c_str());
  if (to_emulator_socket_.handle() == nullptr) {
    throw std::runtime_error("Failed to connect to endpoint");
  }

  server_thread_ =
      std::thread{&ZmqTransport::ServerThread, this, from_emulator};
}

ZmqTransport::~ZmqTransport() {
  try {
    running_ = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    from_emulator_context_.shutdown();
    from_emulator_context_.close();

    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  } catch (const zmq::error_t& e) {
    // Shutting down
    if (e.num() != ETERM) {
      std::cout << "Error: " << e.what() << '\n';
    }
  } catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << '\n';
  } catch (...) {
    std::cout << "Unknown error" << '\n';
  }
}

void ZmqTransport::ServerThread(const std::string& endpoint) {
  zmq::socket_t socket{from_emulator_context_, zmq::socket_type::pair};
  socket.bind(endpoint);
  while (running_) {
    std::array<zmq::pollitem_t, 1> items = {
        {{.socket = static_cast<void*>(socket),
          .fd = 0,
          .events = ZMQ_POLLIN,
          .revents = 0}}};

    const int ret{zmq::poll(items.data(), 1, std::chrono::milliseconds{50})};

    if (ret == 0) {
      // Timeout occurred, check the stop condition
      if (!running_) {
        break;
      }
    } else if (ret > 0) {
      zmq::message_t request{};
      if (socket.recv(request, zmq::recv_flags::none)) {
        std::cout << "Received: " << request.to_string() << '\n';
        const std::string_view request_str{
            static_cast<const char*>(request.data()), request.size()};
        if (request_str == "Hello") {
          socket.send(zmq::str_buffer("World"), zmq::send_flags::none);
        } else {
          std::cout << "Dispatching\n";
          auto response = dispatcher_.Dispatch(request.to_string());
          if (response) {
            zmq::message_t reply{response.value().data(),
                                 response.value().size()};
            socket.send(reply, zmq::send_flags::none);
          } else {
            zmq::message_t reply{"Unhandled", 9};
            socket.send(reply, zmq::send_flags::none);
          }
        }
      }
    }
  }
}

auto ZmqTransport::Send(std::string_view data)
    -> std::expected<void, common::Error> {
  if (!to_emulator_socket_.send(zmq::buffer(data), zmq::send_flags::dontwait)) {
    return std::unexpected(common::Error::kOperationFailed);
  }
  return {};
}

auto ZmqTransport::Receive() -> std::expected<std::string, common::Error> {
  zmq::message_t msg{};
  if (to_emulator_socket_.recv(msg, zmq::recv_flags::none) != msg.size()) {
    return std::unexpected(common::Error::kOperationFailed);
  }
  return msg.to_string();
}

}  // namespace mcu
