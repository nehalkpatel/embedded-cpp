#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <libs/common/error.hpp>
#include <thread>
#include <zmq.hpp>

#include "dispatcher.hpp"
#include "zmq_transport.hpp"

namespace mcu {
namespace {

// Deliberately not the emulator's real endpoints. This fixture used to bind
// ipc:///tmp/device_emulator.ipc -- byte-identical to HostBoard::Endpoints and
// to DeviceEmulator's defaults -- so running the unit tests while an emulator
// or blinky was up had them fighting over the same paths.
constexpr auto kEmulatorEndpoint =
    "ipc:///tmp/test_transport_device_emulator.ipc";
constexpr auto kDeviceEndpoint =
    "ipc:///tmp/test_transport_emulator_device.ipc";

class ZmqTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Bind on the test thread, before the serving thread exists. Create() in
    // the test body then happens-after this bind by thread creation alone.
    // Previously the fixture launched a thread that bound the endpoint and the
    // test called Create() immediately, racing it -- nothing ordered the two,
    // and only ZMQ's connect retry hid the race.
    socket_.set(zmq::sockopt::linger, 0);
    socket_.bind(kEmulatorEndpoint);
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

  zmq::context_t context_{1};
  zmq::socket_t socket_{context_, zmq::socket_type::pair};
  std::thread server_thread_;
  std::atomic<bool> running_{true};
};

TEST_F(ZmqTransportTest, SendReceive) {
  const ReceiverMap receiver_map{};
  Dispatcher dispatcher{receiver_map};
  auto transport =
      mcu::ZmqTransport::Create(kEmulatorEndpoint, kDeviceEndpoint, dispatcher);
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

}  // namespace
}  // namespace mcu
