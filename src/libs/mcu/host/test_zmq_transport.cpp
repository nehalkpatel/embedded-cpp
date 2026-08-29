#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <libs/common/error.hpp>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "libs/common/logger.hpp"
#include "libs/mcu/host/test_support.hpp"
#include "zmq_transport.hpp"

namespace mcu {
namespace {

auto Endpoint(std::string_view role) -> std::string {
  return test::MakeEndpoint("test_transport", role);
}

class ZmqTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Bind on the test thread, before the serving thread exists. Create() in
    // the test body then happens-after this bind by thread creation alone.
    // Previously the fixture launched a thread that bound the endpoint and the
    // test called Create() immediately, racing it -- nothing ordered the two,
    // and only ZMQ's connect retry hid the race.
    socket_.set(zmq::sockopt::linger, 0);
    socket_.bind(emulator_endpoint_);
    server_thread_ = std::thread{[this]() { ServerLoop(); }};
  }

  void TearDown() override {
    // Order matters. The old teardown terminated the context first and slept
    // 100ms hoping the thread would notice; a context terminated out from under
    // zmq::poll throws ETERM straight out of a thread with no handler, which is
    // std::terminate. Stopping the loop and joining first removes both the
    // sleep and the hazard. The socket must close before its context, or
    // context teardown blocks waiting for it.
    running_ = false;
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
    socket_.close();
    context_.close();

    test::RemoveEndpointArtifacts(emulator_endpoint_);
    test::RemoveEndpointArtifacts(device_endpoint_);
  }

 private:
  void ServerLoop() {
    while (running_) {
      std::array<zmq::pollitem_t, 1> items = {
          {{.socket = static_cast<void*>(socket_),
            .fd = 0,
            .events = ZMQ_POLLIN,
            .revents = 0}}};

      // Bounded, so the loop notices running_ within one interval and the
      // join above never waits long.
      const int ret{zmq::poll(items.data(), 1, std::chrono::milliseconds{50})};

      if (ret <= 0) {
        continue;
      }

      zmq::message_t request{};
      if (socket_.recv(request, zmq::recv_flags::none)) {
        const std::string_view request_str{
            static_cast<const char*>(request.data()), request.size()};
        if (request_str == "Hello") {
          socket_.send(zmq::str_buffer("World"), zmq::send_flags::none);
        }
      } else {
        socket_.send(zmq::str_buffer("Unknown"), zmq::send_flags::none);
      }
    }
  }

 protected:
  const std::string emulator_endpoint_{Endpoint("device_emulator")};
  const std::string device_endpoint_{Endpoint("emulator_device")};

 private:
  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, zmq::socket_type::pair};
  std::thread server_thread_;
  std::atomic<bool> running_{true};
};

TEST_F(ZmqTransportTest, SendReceive) {
  const ReceiverMap receiver_map{};
  Dispatcher dispatcher{receiver_map};
  auto transport = mcu::ZmqTransport::Create(emulator_endpoint_,
                                             device_endpoint_, dispatcher);
  // has_value() rather than the expected itself: std::expected's operator bool
  // is explicit, so gtest's AssertionResult will not take it.
  ASSERT_TRUE(transport.has_value());
  EXPECT_TRUE((*transport)->IsReady());

  auto result = (*transport)->Send("Hello");
  ASSERT_TRUE(result);
  auto response = (*transport)->Receive();
  ASSERT_TRUE(response);
  ASSERT_EQ(response.value(), "World");
}

// Drives Send() into genuine backpressure, and reports the send that met it.
//
// The obvious setup -- point the transport at an endpoint nobody binds -- does
// NOT work, despite the mute state PAIR sockets are documented to have.
// connect() creates the outbound pipe immediately whether or not a peer is
// reachable, and libzmq queues into it, so a peerless send returns success.
// Backpressure begins only once ZMQ_SNDHWM messages (1000 by default) are
// outstanding, and that is where the retry loop finally has something to
// absorb. Measuring the first send to block gives the retry path a clean
// reading: every send before it succeeded on its first attempt and logged
// nothing.
struct BlockedSend {
  std::expected<void, common::Error> result;
  std::chrono::steady_clock::duration elapsed;
  int sends;
};

auto SendUntilQueueBlocks(ZmqTransport& transport) -> BlockedSend {
  // Comfortably past the default high-water mark, and a backstop against a
  // regression that made sends succeed forever rather than hanging the suite.
  constexpr int kMaxSends{5000};

  BlockedSend blocked{};
  for (blocked.sends = 1; blocked.sends <= kMaxSends; ++blocked.sends) {
    const auto start{std::chrono::steady_clock::now()};
    blocked.result = transport.Send("Hello");
    blocked.elapsed = std::chrono::steady_clock::now() - start;
    if (!blocked.result) {
      break;
    }
  }
  return blocked;
}

class ZmqTransportRetryTest : public ::testing::Test {
 protected:
  void TearDown() override { test::RemoveEndpointArtifacts(own_endpoint_); }

  static auto MakeConfig(
      common::Logger& logger, std::chrono::milliseconds send_timeout,
      std::chrono::milliseconds total_timeout) -> TransportConfig {
    return TransportConfig{
        .send_timeout = send_timeout,
        .retry = {.max_attempts = kAttempts,
                  .retry_delay = kRetryDelay,
                  .total_timeout = total_timeout},
        .logger = logger,
    };
  }

  static constexpr uint32_t kAttempts{3};
  static constexpr std::chrono::milliseconds kRetryDelay{10};

  const std::string absent_peer_endpoint_{Endpoint("absent_peer")};
  const std::string own_endpoint_{Endpoint("retry_own")};
  test::RecordingLogger logger_;
  const ReceiverMap receiver_map_;
};

// The regression test the fix owes: a blocked send must make every attempt it
// was configured for.
//
// Three assertions that fail independently, because each admits a different
// wrong answer on its own. kTimeout says the outcome was classified as
// retryable at all -- before the fix an expired ZMQ_SNDTIMEO came back as a
// falsy result rather than an exception and was reported as kOperationFailed,
// so the loop never even reached its deadline check. The log count says the
// loop iterated. The elapsed time says each iteration blocked on the socket for
// its own timeout instead of failing instantly.
TEST_F(ZmqTransportRetryTest, SendAttemptsEveryRetryWhenSocketBlocks) {
  constexpr std::chrono::milliseconds kSendTimeout{100};
  constexpr std::chrono::milliseconds kTotalTimeout{2000};

  Dispatcher dispatcher{receiver_map_};
  auto config = MakeConfig(logger_, kSendTimeout, kTotalTimeout);
  auto transport = ZmqTransport::Create(absent_peer_endpoint_, own_endpoint_,
                                        dispatcher, config);
  ASSERT_TRUE(transport.has_value());
  ASSERT_TRUE((*transport)->IsReady());

  const auto blocked = SendUntilQueueBlocks(**transport);

  ASSERT_FALSE(blocked.result.has_value())
      << "no send ever met backpressure in " << blocked.sends << " attempts";
  EXPECT_EQ(blocked.result.error(), common::Error::kTimeout);
  EXPECT_EQ(logger_.Count("Send retrying"), kAttempts - 1)
      << "the retry loop ran " << logger_.Count("Send retrying") + 1
      << " of its " << kAttempts << " configured attempts";
  // Lower bound only: an upper bound here would measure the machine's load
  // rather than this code. Two attempts' worth of blocking cannot fit in one.
  EXPECT_GE(blocked.elapsed, 2 * kSendTimeout);
  EXPECT_LT(blocked.elapsed, kTotalTimeout) << "the whole-send budget overran";
}

// The shape of the configuration that shipped: one attempt allowed to consume
// the entire retry budget, so the first EAGAIN arrives at the deadline and
// max_attempts and retry_delay describe nothing that can happen.
//
// The transport now shrinks the per-attempt slice until the configured attempts
// fit, so the caller's stated intent -- three tries within this budget -- is
// what they get.
TEST_F(ZmqTransportRetryTest, SendRetriesWhenSendTimeoutClaimsTheWholeBudget) {
  constexpr std::chrono::milliseconds kBudget{600};
  // (600ms - 2 * 10ms of retry delay) / 3 attempts.
  constexpr std::chrono::milliseconds kExpectedSlice{193};

  Dispatcher dispatcher{receiver_map_};
  auto config = MakeConfig(logger_, kBudget, kBudget);
  auto transport = ZmqTransport::Create(absent_peer_endpoint_, own_endpoint_,
                                        dispatcher, config);
  ASSERT_TRUE(transport.has_value());
  ASSERT_TRUE((*transport)->IsReady());

  const auto blocked = SendUntilQueueBlocks(**transport);

  ASSERT_FALSE(blocked.result.has_value())
      << "no send ever met backpressure in " << blocked.sends << " attempts";
  EXPECT_EQ(blocked.result.error(), common::Error::kTimeout);
  EXPECT_EQ(logger_.Count("send_timeout exceeds"), 1U)
      << "an incoherent budget should be clamped, and said so";
  EXPECT_EQ(logger_.Count("Send retrying"), kAttempts - 1);
  // Bounded both ways, each side ruling out a different wrong answer. Below:
  // the attempts genuinely blocked rather than failing instantly. Above: they
  // were shortened to share the budget, not repeated at full length -- which
  // would have taken three times as long.
  EXPECT_GE(blocked.elapsed, 2 * kExpectedSlice);
  EXPECT_LT(blocked.elapsed, 2 * kBudget);
}

}  // namespace
}  // namespace mcu
