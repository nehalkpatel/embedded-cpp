#pragma once

#include <condition_variable>
#include <expected>
#include <thread>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "libs/common/error.hpp"
#include "libs/common/logger.hpp"
#include "transport.hpp"

namespace mcu {

enum class TransportState {
  kDisconnected,
  kConnecting,
  kConnected,
  kError,
};

struct RetryConfig {
  uint32_t max_attempts{3};
  std::chrono::milliseconds retry_delay{10};
  std::chrono::milliseconds total_timeout{1000};
};

struct TransportConfig {
  std::chrono::milliseconds poll_timeout{50};
  std::chrono::milliseconds connect_timeout{5000};
  std::chrono::milliseconds shutdown_timeout{2000};
  std::chrono::milliseconds send_timeout{1000};
  std::chrono::milliseconds recv_timeout{5000};
  int linger_ms{0};  // Discard pending messages on close
  RetryConfig retry{};
  common::Logger& logger;  // Logger reference (defaults to NullLogger)

  // Default constructor uses NullLogger
  TransportConfig() : logger(GetDefaultLogger()) {}

  // Allow custom logger via dependency injection
  explicit TransportConfig(common::Logger& custom_logger)
      : logger(custom_logger) {}

 private:
  static auto GetDefaultLogger() -> common::Logger& {
    static common::NullLogger null_logger{};
    return null_logger;
  }
};

class ZmqTransport : public Transport {
 public:
  ZmqTransport() = delete;
  ZmqTransport(const ZmqTransport&) = delete;
  ZmqTransport(ZmqTransport&&) = delete;
  auto operator=(const ZmqTransport&) -> ZmqTransport& = delete;
  auto operator=(ZmqTransport&&) -> ZmqTransport& = delete;
  ~ZmqTransport() override;

  auto Send(std::string_view data)
      -> std::expected<void, common::Error> override;
  auto Receive() -> std::expected<std::string, common::Error> override;

  // New methods for connection management
  auto State() const -> TransportState { return state_.load(); }
  auto IsConnected() const -> bool {
    return state_.load() == TransportState::kConnected;
  }
  auto WaitForConnection(std::chrono::milliseconds timeout)
      -> std::expected<void, common::Error>;
  // Factory method - preferred way to create transport
  static auto Create(const std::string& to_emulator,
                     const std::string& from_emulator, Dispatcher& dispatcher,
                     const TransportConfig& config = {})
      -> std::expected<std::unique_ptr<ZmqTransport>, common::Error>;

  // Constructor - prefer using Create() factory method
  ZmqTransport(const std::string& to_emulator, const std::string& from_emulator,
               Dispatcher& dispatcher, const TransportConfig& config = {});

 private:
  auto ServerThread(const std::string& endpoint) -> void;
  auto SetSocketOptions() -> void;

  // Logging helpers to reduce cognitive complexity
  auto LogDebug(std::string_view msg) const -> void {
    config_.logger.Debug(msg);
  }
  auto LogInfo(std::string_view msg) const -> void { config_.logger.Info(msg); }
  auto LogWarning(std::string_view msg) const -> void {
    config_.logger.Warning(msg);
  }
  auto LogError(std::string_view msg) const -> void {
    config_.logger.Error(msg);
  }

  TransportConfig config_;
  std::atomic<TransportState> state_{TransportState::kDisconnected};

  zmq::context_t to_emulator_context_{1};
  zmq::socket_t to_emulator_socket_{to_emulator_context_,
                                    zmq::socket_type::pair};
  zmq::context_t from_emulator_context_{1};

  std::atomic<bool> running_{true};
  std::condition_variable shutdown_cv_;
  std::mutex shutdown_mutex_;

  Dispatcher& dispatcher_;
  std::thread server_thread_;
};

}  // namespace mcu
