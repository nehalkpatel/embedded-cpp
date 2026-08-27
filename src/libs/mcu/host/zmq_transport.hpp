#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "libs/common/error.hpp"
#include "libs/common/logger.hpp"
#include "transport.hpp"

namespace mcu {

// What the transport knows about ITSELF.
//
// No value here claims anything about the peer process. Both sockets are
// ZMQ_PAIR, and ZMQ offers no connection callback without a socket monitor,
// which this class deliberately does not use. Whether a peer is attached is
// only ever knowable from the result of the operation you just attempted --
// which is why a missing emulator surfaces as Send() returning kTimeout rather
// than as a state value.
enum class TransportState : uint8_t {
  // Constructed, startup not yet begun. Not observable from outside: the
  // constructor moves to kStarting before any other thread holds a reference.
  kUninitialized,
  // Server thread launched; the bind outcome is not yet known.
  kStarting,
  // Our receive socket is bound and connect() has been issued on our send
  // socket. The only state in which Send()/Receive() are permitted.
  //
  // This does NOT mean a peer is attached. connect() is asynchronous and
  // returns before any peer exists, so a Send() in this state can still sit in
  // ZMQ's mute state for send_timeout and come back as kTimeout.
  kReady,
  // Startup failed; terminal. Send()/Receive() return kInvalidState from here
  // on, and StartupStatus() carries the reason.
  kFailed,
};

struct RetryConfig {
  uint32_t max_attempts{3};
  std::chrono::milliseconds retry_delay{10};
  std::chrono::milliseconds total_timeout{1000};
};

struct TransportConfig {
  std::chrono::milliseconds poll_timeout{50};
  // Bounds the one wait the constructor performs: the server thread's bind
  // handshake. There is deliberately no connect timeout to go with it --
  // connect() on a PAIR socket is asynchronous and completes without a peer,
  // so there would be nothing to wait for.
  std::chrono::milliseconds startup_timeout{5000};
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

  auto State() const -> TransportState { return state_.load(); }
  auto IsReady() const -> bool {
    return state_.load() == TransportState::kReady;
  }

  // Outcome of the startup sequence the constructor ran. Never blocks: by the
  // time the constructor has returned, startup has already succeeded, failed,
  // or timed out.
  auto StartupStatus() const -> std::expected<void, common::Error> {
    const auto error{startup_error_.load()};
    if (error != common::Error::kOk) {
      return std::unexpected(error);
    }
    return {};
  }

  // Factory method - preferred way to create transport
  static auto Create(const std::string& to_emulator,
                     const std::string& from_emulator, Dispatcher& dispatcher,
                     const TransportConfig& config = {})
      -> std::expected<std::unique_ptr<ZmqTransport>, common::Error>;

  // Never throws, and never blocks past config.startup_timeout. On failure the
  // object is still fully constructed but permanently unusable: State() is
  // kFailed, every Send()/Receive() returns kInvalidState, and StartupStatus()
  // carries the reason. Prefer Create(), which makes that check for you.
  ZmqTransport(const std::string& to_emulator, const std::string& from_emulator,
               Dispatcher& dispatcher, const TransportConfig& config = {});

 private:
  enum class BindOutcome : uint8_t { kPending, kBound, kFailed };

  auto ServerThread(const std::string& endpoint) -> void;
  auto ServeLoop(zmq::socket_t& socket) -> void;
  auto SignalBind(BindOutcome outcome) -> void;
  auto AwaitBind() -> BindOutcome;
  auto FailStartup(common::Error error, std::string_view msg) -> void;
  auto EndpointHasLiveOwner(const std::string& endpoint) const -> bool;
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
  std::atomic<TransportState> state_{TransportState::kUninitialized};
  std::atomic<common::Error> startup_error_{common::Error::kOk};

  zmq::context_t to_emulator_context_{1};
  zmq::socket_t to_emulator_socket_{to_emulator_context_,
                                    zmq::socket_type::pair};
  zmq::context_t from_emulator_context_{1};

  std::atomic<bool> running_{true};
  BindOutcome bind_outcome_{BindOutcome::kPending};  // guarded by bind_mutex_
  std::condition_variable bind_cv_;
  std::mutex bind_mutex_;

  Dispatcher& dispatcher_;
  std::thread server_thread_;
};

}  // namespace mcu
