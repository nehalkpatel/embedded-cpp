#include "zmq_transport.hpp"

#include <algorithm>
#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "endpoint_lock.hpp"
#include "libs/common/error.hpp"

namespace mcu {

auto ZmqTransport::Create(const std::string& to_emulator,
                          const std::string& from_emulator,
                          Dispatcher& dispatcher, const TransportConfig& config)
    -> std::expected<std::unique_ptr<ZmqTransport>, common::Error> {
  try {
    config.logger.get().Info("Creating ZmqTransport");

    auto transport{std::make_unique<ZmqTransport>(to_emulator, from_emulator,
                                                  dispatcher, config)};

    if (auto status{transport->StartupStatus()}; !status) {
      config.logger.get().Error("ZmqTransport startup failed");
      return std::unexpected(status.error());
    }

    config.logger.get().Info("ZmqTransport created successfully");
    return transport;
  } catch (...) {  // NOLINT
    // The constructor does not throw, so this covers allocation failure only.
    config.logger.get().Error("Unknown error during creation");
    return std::unexpected(common::Error::kUnknown);
  }
}

ZmqTransport::ZmqTransport(const std::string& to_emulator,    // NOLINT
                           const std::string& from_emulator,  // NOLINT
                           Dispatcher& dispatcher,
                           const TransportConfig& config)
    : config_{config}, dispatcher_{dispatcher} {
  LogDebug("Initializing ZmqTransport");

  state_ = TransportState::kStarting;

  // Nothing below may escape as an exception. Once server_thread_ is running,
  // an exception leaving this constructor destroys a *joinable* std::thread,
  // which calls std::terminate -- Create()'s handler never gets a look in.
  try {
    ClampSendTimeoutToRetryBudget();
    SetSocketOptions();

    // Start server thread FIRST (it will BIND)
    server_thread_ =
        std::thread{&ZmqTransport::ServerThread, this, from_emulator};

    switch (AwaitBind()) {
      case BindOutcome::kPending:
        FailStartup(common::Error::kTimeout,
                    "Startup timed out waiting for the server socket to bind");
        return;
      case BindOutcome::kFailed:
        FailStartup(common::Error::kOperationFailed,
                    "Startup failed: could not bind the server socket");
        return;
      case BindOutcome::kBound:
        break;
    }

    // Now CONNECT to emulator (emulator should already be bound)
    LogDebug("Connecting to emulator");
    to_emulator_socket_.connect(to_emulator);

    state_ = TransportState::kReady;
    LogDebug("ZmqTransport ready (peer liveness unknown)");
  } catch (const zmq::error_t&) {
    FailStartup(common::Error::kConnectionRefused, "Startup failed: ZMQ error");
  } catch (...) {  // NOLINT
    FailStartup(common::Error::kUnknown, "Startup failed: unknown error");
  }
}

auto ZmqTransport::FailStartup(common::Error error,
                               std::string_view msg) -> void {
  startup_error_.store(error);
  state_.store(TransportState::kFailed);
  LogError(msg);
}

auto ZmqTransport::AwaitBind() -> BindOutcome {
  std::unique_lock<std::mutex> lock(bind_mutex_);
  // Bounded on purpose. If the server thread dies before it can bind -- a stale
  // ipc file, a second instance already holding the endpoint -- bind_outcome_
  // stays kPending, and an unbounded wait here hangs the constructor forever.
  bind_cv_.wait_for(lock, config_.startup_timeout, [this]() {
    return bind_outcome_ != BindOutcome::kPending;
  });
  return bind_outcome_;
}

auto ZmqTransport::SignalBind(BindOutcome outcome) -> void {
  {
    const std::lock_guard<std::mutex> lock(bind_mutex_);
    if (bind_outcome_ != BindOutcome::kPending) {
      return;  // First writer wins.
    }
    bind_outcome_ = outcome;
  }
  bind_cv_.notify_all();
}

// Makes RetryConfig's invariant true rather than merely documented.
//
// ZMQ_SNDTIMEO bounds a single attempt; retry.total_timeout bounds the whole
// Send(). If one attempt may consume the entire budget then the first EAGAIN
// arrives at or after the deadline and Send() returns having tried once --
// max_attempts and retry_delay become dead configuration.
//
// Shrinking the per-attempt slice is preferred to rejecting the config. A
// caller who asks for three attempts within a second has said something
// coherent about what they want; the arithmetic that makes it fit is ours to
// do, and failing startup over a tuning number would be a worse answer.
auto ZmqTransport::ClampSendTimeoutToRetryBudget() -> void {
  // Zero attempts would skip the loop entirely and report a timeout without
  // ever touching the socket.
  config_.retry.max_attempts = std::max(config_.retry.max_attempts, 1U);
  const auto attempts{config_.retry.max_attempts};

  const auto delays{(attempts - 1) * config_.retry.retry_delay};
  const auto sending{delays < config_.retry.total_timeout
                         ? config_.retry.total_timeout - delays
                         : std::chrono::milliseconds::zero()};
  // A floor of 1ms, because 0 means "never block" and a negative value means
  // "block forever" -- both worse than a very short attempt.
  const auto slice{std::max(std::chrono::milliseconds{1}, sending / attempts)};

  if (config_.send_timeout > slice) {
    LogWarning("send_timeout exceeds the per-attempt retry budget; clamping");
    config_.send_timeout = slice;
  }
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

// One attempt at the socket, classified.
//
// The classification is the substance here. cppzmq's send() reports an expired
// ZMQ_SNDTIMEO by returning an EMPTY result and throws error_t only for
// everything else -- so the timeout this retry loop exists to absorb is a
// falsy return, not an exception, and both paths must map to kWouldBlock.
//
// ETIMEDOUT is caught for the same outcome: no libzmq version in use raises it
// here, but it means precisely what EAGAIN means and costs one line.
auto ZmqTransport::TrySendOnce(std::string_view data) -> SendAttempt {
  try {
    const auto result{
        to_emulator_socket_.send(zmq::buffer(data), zmq::send_flags::none)};
    return result ? SendAttempt::kSent : SendAttempt::kWouldBlock;
  } catch (const zmq::error_t& e) {
    if (e.num() == EAGAIN || e.num() == ETIMEDOUT) {
      return SendAttempt::kWouldBlock;
    }
    return SendAttempt::kFailed;
  }
}

auto ZmqTransport::Send(std::string_view data)
    -> std::expected<void, common::Error> {
  if (state_.load() != TransportState::kReady) {
    LogWarning("Send failed: transport not ready");
    return std::unexpected(common::Error::kInvalidState);
  }

  // The whole-send budget. Each attempt is separately bounded by the socket's
  // ZMQ_SNDTIMEO, which the constructor sized to fit max_attempts of them in
  // here; this deadline is the backstop for a peer that keeps us just under it.
  const auto deadline{std::chrono::steady_clock::now() +
                      config_.retry.total_timeout};

  for (uint32_t attempt = 0; attempt < config_.retry.max_attempts; ++attempt) {
    switch (TrySendOnce(data)) {
      case SendAttempt::kSent:
        if (attempt > 0) {
          LogDebug("Send succeeded after retry");
        }
        return {};
      case SendAttempt::kFailed:
        LogError("Send failed with non-retryable error");
        return std::unexpected(common::Error::kOperationFailed);
      case SendAttempt::kWouldBlock:
        break;
    }

    if (attempt + 1 >= config_.retry.max_attempts) {
      break;  // Out of attempts.
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      break;  // Out of budget.
    }
    LogDebug("Send retrying after transient error");
    std::this_thread::sleep_for(config_.retry.retry_delay);
  }

  // Every exhausted path is a timeout: a mute PAIR socket blocks rather than
  // dropping, so running out of attempts and running out of budget both mean
  // the peer never took the message.
  LogError("Send timeout after retries");
  return std::unexpected(common::Error::kTimeout);
}

auto ZmqTransport::ServerThread(const std::string& endpoint) -> void {
  LogDebug("ServerThread starting");

  zmq::socket_t socket{};

  // The bind phase. Every exit from this block -- fallthrough, early return, or
  // exception -- must publish an outcome: the constructor is parked in
  // AwaitBind(), and only a published outcome releases it before the timeout.
  try {
    socket = zmq::socket_t{from_emulator_context_, zmq::socket_type::pair};
    socket.set(zmq::sockopt::linger, config_.linger_ms);
    socket.set(zmq::sockopt::rcvtimeo,
               static_cast<int>(config_.poll_timeout.count()));

    // Two checks, and the order matters. The lock is the guarantee: it is
    // atomic, so it excludes every other transport that plays by the same
    // rules, with no window between deciding and binding. The probe is the
    // fallback for an owner that does not -- an older build, or anything else
    // that happens to be listening on that path.
    if (!endpoint_lock_.TryAcquire(endpoint)) {
      LogError("Refusing to bind: endpoint is locked by another live process");
      SignalBind(BindOutcome::kFailed);
      return;
    }
    if (EndpointHasLiveOwner(endpoint)) {
      LogError("Refusing to bind: endpoint is served by another live process");
      SignalBind(BindOutcome::kFailed);
      return;
    }
    socket.bind(endpoint);
  } catch (const zmq::error_t&) {
    LogError("ServerThread failed to bind");
    SignalBind(BindOutcome::kFailed);
    return;
  } catch (...) {  // NOLINT
    LogError("ServerThread failed to bind with an unknown error");
    SignalBind(BindOutcome::kFailed);
    return;
  }

  SignalBind(BindOutcome::kBound);
  LogDebug("ServerThread bound and listening");

  ServeLoop(socket);

  LogDebug("ServerThread exiting");
}

auto ZmqTransport::ServeLoop(zmq::socket_t& socket) -> void {
  while (running_) {
    try {
      zmq::message_t request{};
      auto result = socket.recv(request, zmq::recv_flags::none);

      if (!result) {
        // Timeout or would block - check running flag
        continue;
      }

      auto response = dispatcher_.Dispatch(request.to_string());
      if (response) {
        zmq::message_t reply{response.value().data(), response.value().size()};
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
        LogDebug("ServeLoop received ETERM, exiting");
        return;
      }
      LogError("ServeLoop ZMQ error");
    } catch (...) {  // NOLINT
      LogError("ServeLoop caught exception");
      return;
    }
  }
}

auto ZmqTransport::Receive() -> std::expected<std::string, common::Error> {
  if (state_.load() != TransportState::kReady) {
    LogWarning("Receive failed: transport not ready");
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
