#include <gtest/gtest.h>

#include <libs/common/error.hpp>
#include <thread>

#include "dispatcher.hpp"
#include "zmq_transport.hpp"

namespace mcu {
namespace {

class ZmqTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_thread_ = std::thread(&ZmqTransportTest::ServerThread, this,
                                 "ipc:///tmp/device_emulator.ipc");
  }

  void TearDown() override {
    try {
      running_ = false;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      context_.shutdown();
      context_.close();

      server_thread_.join();
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

 private:
  void ServerThread(const std::string& endpoint) {
    zmq::socket_t socket{context_, zmq::socket_type::pair};
    socket.bind(endpoint);
    while (running_) {
      std::array<zmq::pollitem_t, 1> items = {
          {{.socket = static_cast<void*>(socket),
            .fd = 0,
            .events = ZMQ_POLLIN,
            .revents = 0}}};

      const int ret = zmq::poll(items.data(), 1, std::chrono::milliseconds{50});

      if (ret == 0) {
        // Timeout occurred, check the stop condition
        if (!running_) {
          break;
        }
      } else if (ret > 0) {
        zmq::message_t request;
        if (socket.recv(request, zmq::recv_flags::none)) {
          const std::string_view request_str{
              static_cast<const char*>(request.data()), request.size()};
          if (request_str == "Hello") {
            socket.send(zmq::str_buffer("World"), zmq::send_flags::none);
          }
        } else {
          socket.send(zmq::str_buffer("Unknown"), zmq::send_flags::none);
        }
      }
    }
  }

  zmq::context_t context_{1};
  std::thread server_thread_;
  std::atomic<bool> running_{true};
};

TEST_F(ZmqTransportTest, SendReceive) {
  ReceiverMap receiver_map{};
  Dispatcher dispatcher{receiver_map};
  ZmqTransport transport{"ipc:///tmp/device_emulator.ipc",
                         "ipc:///tmp/emulator_device.ipc", dispatcher};
  auto result = transport.Send("Hello");
  ASSERT_TRUE(result);
  auto response = transport.Receive();
  ASSERT_TRUE(response);
  ASSERT_EQ(response.value(), "World");
}

TEST(ZmqTransport, ClientMessage) {
  ReceiverMap receiver_map{};
  Dispatcher dispatcher{receiver_map};
  const ZmqTransport transport{"ipc:///tmp/device_emulator.ipc",
                               "ipc:///tmp/emulator_device.ipc", dispatcher};
  zmq::context_t context{1};
  zmq::socket_t socket{context, zmq::socket_type::pair};
  socket.connect("ipc:///tmp/emulator_device.ipc");
  socket.send(zmq::str_buffer("Hello"), zmq::send_flags::none);
  zmq::message_t response;
  ASSERT_GT(socket.recv(response, zmq::recv_flags::none), 0);
  const std::string_view response_str{static_cast<const char*>(response.data()),
                                      response.size()};
  ASSERT_EQ(response_str, "World");
}

}  // namespace
}  // namespace mcu
