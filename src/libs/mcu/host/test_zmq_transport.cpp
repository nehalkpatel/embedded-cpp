#include <gtest/gtest.h>

#include <thread>

#include "zmq_transport.hpp"

class ZmqTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::thread server_thread{[this]() {
      zmq::context_t context{1};
      zmq::socket_t socket{context, zmq::socket_type::pair};
      socket.bind("ipc:///tmp/device_emulator.ipc");
      while (true) {
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
    }};
    server_thread.detach();
  }
};

TEST_F(ZmqTransportTest, SendReceive) {
  mcu::ZmqTransport transport{"ipc:///tmp/device_emulator.ipc"};
  auto result = transport.Send("Hello");
  ASSERT_TRUE(result);
  auto response = transport.Receive();
  ASSERT_TRUE(response);
  ASSERT_EQ(response.value(), "World");
}