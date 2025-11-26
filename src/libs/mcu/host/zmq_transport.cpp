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
    config.logger.Info("Creating ZmqTransport");

    auto transport{std::make_unique<ZmqTransport>(to_emulator, from_emulator,
                                                  dispatcher, config)};

    // Wait for connection to establish
    auto wait_result{transport->WaitForConnection(config.connect_timeout)};
    if (!wait_result) {
      config.logger.Error("Connection timeout");
      return std::unexpected(wait_result.error());
    }

    config.logger.Info("ZmqTransport created successfully");
    return transport;
  } catch (const zmq::error_t& /*e*/) {
    config.logger.Error("ZMQ error during creation");
    return std::unexpected(common::Error::kConnectionRefused);
  } catch (...) {
    config.logger.Error("Unknown error during creation");
    return std::unexpected(common::Error::kUnknown);
  }
}

ZmqTransport::ZmqTransport(const std::string& to_emulator,    // NOLINT
                           const std::string& from_emulator,  // NOLINT
                           Dispatcher& dispatcher,
                           const TransportConfig& config)
    : config_{config}, dispatcher_{dispatcher} {
  LogDebug("Initializing ZmqTransport");

  SetSocketOptions();

  state_ = TransportState::kConnecting;

  // Start server thread FIRST (it will BIND)
  server_thread_ =
      std::thread{&ZmqTransport::ServerThread, this, from_emulator};

  // Small sleep to let server thread bind (ZMQ binding is fast, ~1-5ms typical)
  // This is a pragmatic approach - alternatives would require condition
  // variables or synchronization primitives which add complexity for minimal
  // benefit
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Now CONNECT to emulator (emulator should already be bound)
  LogDebug("Connecting to emulator");
  to_emulator_socket_.connect(to_emulator.c_str());

  state_ = TransportState::kConnected;
  LogDebug("ZmqTransport initialized");
}

auto ZmqTransport::SetSocketOptions() -> void {
  // Set linger to 0 to discard messages immediately on close
  to_emulator_socket_.set(zmq::sockopt::linger, config_.linger_ms);

  // Set send/recv timeouts from configuration
  to_emulator_socket_.set(zmq::sockopt::sndtimeo,
                          static_cast<int>(config_.send_timeout.count()));
  to_emulator_socket_.set(zmq::sockopt::rcvtimeo,
                          static_cast<int>(config_.recv_timeout.count()));
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
    LogDebug("Shutting down ZmqTransport");

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

    LogDebug("ZmqTransport shutdown complete");

  } catch (const zmq::error_t& e) {
    if (e.num() != ETERM) {
      LogError("ZMQ error during shutdown");
    }
  } catch (...) {  // NOLINT
    // Suppress all exceptions in destructor
  }
}

auto ZmqTransport::Send(std::string_view data)
    -> std::expected<void, common::Error> {
  if (state_ != TransportState::kConnected) {
    LogWarning("Send failed: not connected");
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
        if (attempt > 0) {
          LogDebug("Send succeeded after retry");
        }
        return {};  // Success!
      }
    } catch (const zmq::error_t& e) {
      // Check if error is retryable
      if (e.num() == EAGAIN || e.num() == ETIMEDOUT) {
        // Check if we've exceeded total timeout
        if (std::chrono::steady_clock::now() >= deadline) {
          LogError("Send timeout after retries");
          return std::unexpected(common::Error::kTimeout);
        }

        if (attempt + 1 < config_.retry.max_attempts) {
          LogDebug("Send retrying after transient error");
        }

        // Wait before retry (unless this was the last attempt)
        if (attempt + 1 < config_.retry.max_attempts) {
          std::this_thread::sleep_for(config_.retry.retry_delay);
        }
        continue;  // Retry
      }

      // Non-retryable error
      LogError("Send failed with non-retryable error");
      return std::unexpected(common::Error::kOperationFailed);
    }

    // result was false but no exception - operation failed
    LogError("Send operation returned false");
    return std::unexpected(common::Error::kOperationFailed);
  }

  // Max attempts exceeded
  LogError("Send failed: max attempts exceeded");
  return std::unexpected(common::Error::kTimeout);
}

void ZmqTransport::ServerThread(const std::string& endpoint) {
  try {
    LogDebug("ServerThread starting");

    zmq::socket_t socket{from_emulator_context_, zmq::socket_type::pair};
    socket.set(zmq::sockopt::linger, config_.linger_ms);
    socket.set(zmq::sockopt::rcvtimeo,
               static_cast<int>(config_.poll_timeout.count()));

    socket.bind(endpoint);

    LogDebug("ServerThread bound and listening");

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
          LogWarning("Unhandled message in dispatcher");
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
          LogDebug("ServerThread received ETERM, exiting");
          break;
        }
        LogError("ServerThread ZMQ error");
      }
    }

    LogDebug("ServerThread exiting");
  } catch (...) {  // NOLINT
    LogError("ServerThread caught exception");
  }
}

auto ZmqTransport::Receive() -> std::expected<std::string, common::Error> {
  if (state_ != TransportState::kConnected) {
    LogWarning("Receive failed: not connected");
    return std::unexpected(common::Error::kInvalidState);
  }

  try {
    zmq::message_t msg{};
    auto result{to_emulator_socket_.recv(msg, zmq::recv_flags::none)};
    if (!result || result.value() != msg.size()) {
      LogError("Receive operation failed");
      return std::unexpected(common::Error::kOperationFailed);
    }
    return msg.to_string();
  } catch (const zmq::error_t& e) {
    if (e.num() == EAGAIN || e.num() == ETIMEDOUT) {
      LogDebug("Receive timeout");
      return std::unexpected(common::Error::kTimeout);
    }
    LogError("Receive failed with ZMQ error");
    return std::unexpected(common::Error::kOperationFailed);
  }
}

}  // namespace mcu
