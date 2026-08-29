#pragma once

// Shared fixture for host-peripheral tests (UART, I2C, ...). Test-only: this
// header is not part of any library's file set.
//
// It owns everything the peripheral protocol does not care about — a fake
// emulator on its own thread, the transport/dispatcher wiring, readiness, and
// endpoint cleanup. A concrete fixture supplies only its protocol:
//   - MakeReceiver() constructs the peripheral under test on the transport
//   - HandleRequest() plays the emulator's side of one exchange

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <zmq.hpp>

#include "libs/mcu/host/dispatcher.hpp"
#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/host/test_support.hpp"
#include "libs/mcu/host/zmq_transport.hpp"

namespace mcu::test {

class HostPeripheralTest : public ::testing::Test {
 protected:
  /// Construct the peripheral under test against `transport` and return it as
  /// the receiver to register with the dispatcher. Called once, from SetUp.
  virtual auto MakeReceiver(Transport& transport) -> Receiver& = 0;

  /// The emulator's side of one exchange: given the raw message the device
  /// sent, return the encoded reply, or nullopt to ignore the message (as the
  /// real emulator ignores anything it cannot decode — which is also what
  /// swallows the readiness probe below).
  virtual auto HandleRequest(std::string_view message)
      -> std::optional<std::string> = 0;

  void SetUp() override {
    // Bind on the test thread, before the emulator thread exists, so the
    // transport's connect() below happens-after the bind by thread creation
    // alone.
    emulator_socket_.set(zmq::sockopt::linger, 0);
    emulator_socket_.bind(emulator_endpoint_);

    emulator_running_ = true;
    emulator_thread_ = std::thread{[this]() { EmulatorLoop(); }};

    // The dispatcher observes the receiver map by reference, so the receiver
    // can be added after the transport exists.
    dispatcher_ = std::make_unique<Dispatcher>(receiver_map_);

    // Assert rather than value_or(nullptr): Create can fail, and a null
    // transport is dereferenced two lines down.
    auto transport_result = ZmqTransport::Create(
        emulator_endpoint_, device_endpoint_, *dispatcher_);
    ASSERT_TRUE(transport_result.has_value());
    device_transport_ = std::move(transport_result.value());

    receiver_map_.emplace_back(MakeReceiver(*device_transport_));

    // Wait for the condition rather than for a duration. On a PAIR socket a
    // send succeeds only once a pipe to the peer exists, so a successful probe
    // IS the readiness signal, and connect latency is absorbed by SNDTIMEO.
    ASSERT_TRUE(device_transport_->Send("probe"));
  }

  void TearDown() override {
    device_transport_.reset();
    dispatcher_.reset();

    // Stop and join before closing anything the thread is using: terminating a
    // context out from under a running loop throws ETERM inside it.
    emulator_running_ = false;
    if (emulator_thread_.joinable()) {
      emulator_thread_.join();
    }
    emulator_socket_.close();
    emulator_context_.close();

    RemoveEndpointArtifacts(emulator_endpoint_);
    RemoveEndpointArtifacts(device_endpoint_);
  }

  // Device -> emulator; the transport connects its send socket here.
  const std::string emulator_endpoint_{
      MakeEndpoint("test_peripheral", "device_emulator")};
  // Emulator -> device; the transport's server thread binds here.
  const std::string device_endpoint_{
      MakeEndpoint("test_peripheral", "emulator_device")};
  std::unique_ptr<ZmqTransport> device_transport_;

 private:
  void EmulatorLoop() {
    try {
      while (emulator_running_) {
        zmq::pollitem_t item{.socket = static_cast<void*>(emulator_socket_),
                             .fd = 0,
                             .events = ZMQ_POLLIN,
                             .revents = 0};
        if (zmq::poll(&item, 1, std::chrono::milliseconds{50}) <= 0) {
          continue;  // Timeout; recheck emulator_running_.
        }

        zmq::message_t message{};
        if (!emulator_socket_.recv(message, zmq::recv_flags::none)) {
          continue;
        }
        const std::string_view message_str{
            static_cast<const char*>(message.data()), message.size()};

        if (const auto reply = HandleRequest(message_str)) {
          emulator_socket_.send(zmq::buffer(*reply), zmq::send_flags::none);
        }
      }
    } catch (const zmq::error_t& e) {
      // Socket closed during shutdown, expected behavior
      if (e.num() != ETERM) {
        throw;
      }
    }
  }

  ReceiverMap receiver_map_;
  std::unique_ptr<Dispatcher> dispatcher_;
  zmq::context_t emulator_context_{1};
  zmq::socket_t emulator_socket_{emulator_context_, zmq::socket_type::pair};
  std::thread emulator_thread_;
  std::atomic<bool> emulator_running_{false};
};

}  // namespace mcu::test
