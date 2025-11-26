#include "zmq_transport.hpp"

#include <expected>
#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "libs/common/error.hpp"

namespace mcu {
auto ZmqTransport::Create(const std::string& to_emulator,
                          const std::string& from_emulator,
                          Dispatcher& dispatcher, const TransportConfig& config)
    -> std::expected<std::unique_ptr<ZmqTransport>, common::Error> {
  try {
    auto transport{std::make_unique<ZmqTransport>(to_emulator, from_emulator,
                                                  dispatcher, config)};

    // Wait for connection to establish
    auto wait_result{transport->WaitForConnection(config.connect_timeout)};
    if (!wait_result) {
      return std::unexpected(wait_result.error());
    }

    return transport;
  } catch (const zmq::error_t& e) {
    return std::unexpected(common::Error::kConnectionRefused);
  } catch (...) {
    return std::unexpected(common::Error::kUnknown);
  }
}

ZmqTransport::ZmqTransport(const std::string& to_emulator,    // NOLINT
                           const std::string& from_emulator,  // NOLINT
                           Dispatcher& dispatcher,
                           const TransportConfig& config)
    : config_{config}, dispatcher_{dispatcher} {
  SetSocketOptions();

  state_ = TransportState::kConnecting;

  // Start server thread FIRST (it will BIND)
  server_thread_ =
      std::thread{&ZmqTransport::ServerThread, this, from_emulator};

  // Small sleep to let server thread bind (ZMQ binding is fast, ~1-5ms typical)
  // This is a pragmatic approach - alternatives would require condition variables
  // or synchronization primitives which add complexity for minimal benefit
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Now CONNECT to emulator (emulator should already be bound)
  to_emulator_socket_.connect(to_emulator.c_str());

  state_ = TransportState::kConnected;
}

auto ZmqTransport::SetSocketOptions() -> void {
  // Set linger to 0 to discard messages immediately on close
  to_emulator_socket_.set(zmq::sockopt::linger, config_.linger_ms);

  // Set send/recv timeouts
  to_emulator_socket_.set(zmq::sockopt::sndtimeo, 1000);  // 1 second
  to_emulator_socket_.set(zmq::sockopt::rcvtimeo, 5000);  // 5 seconds
}

auto ZmqTransport::WaitForConnection(std::chrono::milliseconds timeout)
    -> std::expected<void, common::Error> {
  const auto deadline{std::chrono::steady_clock::now() + timeout};

  while (state_ == TransportState::kConnecting) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return std::unexpected(common::Error::kTimeout);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (state_ == TransportState::kError) {
    return std::unexpected(common::Error::kConnectionRefused);
  }

  return {};
}

ZmqTransport::~ZmqTransport() {
  try {
    // Signal shutdown
    running_ = false;

    // Shutdown context (will unblock recv/send operations in ServerThread)
    from_emulator_context_.shutdown();

    // Join thread - context.shutdown() will cause recv() to throw,
    // which will exit the ServerThread loop
    if (server_thread_.joinable()) {
      server_thread_.join();
    }

    // Close contexts after thread has finished
    from_emulator_context_.close();

  } catch (const zmq::error_t& e) {
    if (e.num() != ETERM) {
      // Log error (use proper logging when available)
    }
  } catch (...) {  // NOLINT
    // Suppress all exceptions in destructor
  }
}

auto ZmqTransport::Send(std::string_view data)
    -> std::expected<void, common::Error> {
  if (state_ != TransportState::kConnected) {
    return std::unexpected(common::Error::kInvalidState);
  }

  // Calculate deadline for retry timeout
  const auto deadline{std::chrono::steady_clock::now() +
                      config_.retry.total_timeout};

  // Retry loop
  for (uint32_t attempt = 0; attempt < config_.retry.max_attempts; ++attempt) {
    try {
      auto result{
          to_emulator_socket_.send(zmq::buffer(data), zmq::send_flags::none)};
      if (result) {
        return {};  // Success!
      }
    } catch (const zmq::error_t& e) {
      // Check if error is retryable
      if (e.num() == EAGAIN || e.num() == ETIMEDOUT) {
        // Check if we've exceeded total timeout
        if (std::chrono::steady_clock::now() >= deadline) {
          return std::unexpected(common::Error::kTimeout);
        }

        // Wait before retry (unless this was the last attempt)
        if (attempt + 1 < config_.retry.max_attempts) {
          std::this_thread::sleep_for(config_.retry.retry_delay);
        }
        continue;  // Retry
      }

      // Non-retryable error
      return std::unexpected(common::Error::kOperationFailed);
    }

    // result was false but no exception - operation failed
    return std::unexpected(common::Error::kOperationFailed);
  }

  // Max attempts exceeded
  return std::unexpected(common::Error::kTimeout);
}

void ZmqTransport::ServerThread(const std::string& endpoint) {
  try {
    zmq::socket_t socket{from_emulator_context_, zmq::socket_type::pair};
    socket.set(zmq::sockopt::linger, config_.linger_ms);
    socket.set(zmq::sockopt::rcvtimeo,
               static_cast<int>(config_.poll_timeout.count()));

    socket.bind(endpoint);

    while (running_) {
      try {
        zmq::message_t request{};
        auto result = socket.recv(request, zmq::recv_flags::none);

        if (!result) {
          // Timeout or would block - check running flag
          continue;
        }

        const std::string_view request_str{
            static_cast<const char*>(request.data()), request.size()};

        auto response = dispatcher_.Dispatch(request.to_string());
        if (response) {
          zmq::message_t reply{response.value().data(),
                               response.value().size()};
          socket.send(reply, zmq::send_flags::none);
        } else {
          zmq::message_t reply{"Unhandled", 9};
          socket.send(reply, zmq::send_flags::none);
        }

      } catch (const zmq::error_t& e) {
        if (e.num() == EAGAIN || e.num() == ETIMEDOUT) {
          // Timeout - normal, check running flag
          continue;
        }
        if (e.num() == ETERM) {
          // Context terminated - time to exit
          break;
        }
        // Other error - log and continue
      }
    }
  } catch (...) {  // NOLINT
    // Thread cleanup
  }
}

auto ZmqTransport::Receive() -> std::expected<std::string, common::Error> {
  if (state_ != TransportState::kConnected) {
    return std::unexpected(common::Error::kInvalidState);
  }

  try {
    zmq::message_t msg{};
    auto result{to_emulator_socket_.recv(msg, zmq::recv_flags::none)};
    if (!result || result.value() != msg.size()) {
      return std::unexpected(common::Error::kOperationFailed);
    }
    return msg.to_string();
  } catch (const zmq::error_t& e) {
    if (e.num() == EAGAIN || e.num() == ETIMEDOUT) {
      return std::unexpected(common::Error::kTimeout);
    }
    return std::unexpected(common::Error::kOperationFailed);
  }
}

}  // namespace mcu
