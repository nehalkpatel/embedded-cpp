#include <gtest/gtest.h>

#include <expected>
#include <string>

#include "dispatcher.hpp"
#include "libs/common/error.hpp"
#include "receiver.hpp"

namespace mcu {
namespace {

constexpr auto AcceptAll(const std::string_view& message) -> bool {
  static_cast<void>(message);
  return true;
}

constexpr auto RejectAll(const std::string_view& message) -> bool {
  static_cast<void>(message);
  return false;
}

constexpr auto IsHello(const std::string_view& message) -> bool {
  return message == "Hello";
}

constexpr auto IsWorld(const std::string_view& message) -> bool {
  return message == "World";
}

class DispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  class SimpleReceiver : public Receiver {
   public:
    auto Receive(const std::string_view& message)
        -> std::expected<std::string, common::Error> override {
      received_message = message;
      return {"Received message"};
    }
    std::string_view received_message;
  };
};

TEST_F(DispatcherTest, DispatchMessage) {
  const std::string sent_message{"Hello"};
  SimpleReceiver receiver;
  ReceiverMap receiver_map{{AcceptAll, receiver}};
  const Dispatcher dispatcher{receiver_map};
  auto reply = dispatcher.Dispatch(sent_message);
  EXPECT_TRUE(reply.has_value());
  EXPECT_EQ(reply.value(), "Received message");
  EXPECT_EQ(receiver.received_message, sent_message);
}

TEST_F(DispatcherTest, DispatchMessageReject) {
  const std::string sent_message{"Hello"};
  SimpleReceiver receiver;
  ReceiverMap receiver_map{{RejectAll, receiver}};
  const Dispatcher dispatcher{receiver_map};
  auto reply = dispatcher.Dispatch(sent_message);
  EXPECT_FALSE(reply.has_value());
  EXPECT_EQ(receiver.received_message, "");
}

TEST_F(DispatcherTest, DispatchMessageMultipleReceivers) {
  const std::string sent_message{"Hello"};
  SimpleReceiver receiver1;
  SimpleReceiver receiver2;
  ReceiverMap receiver_map{{IsHello, receiver1}, {IsWorld, receiver2}};
  const Dispatcher dispatcher{receiver_map};
  auto reply = dispatcher.Dispatch(sent_message);
  EXPECT_TRUE(reply.has_value());
  EXPECT_EQ(reply.value(), "Received message");
  EXPECT_EQ(receiver1.received_message, sent_message);
  EXPECT_EQ(receiver2.received_message, "");
}

TEST_F(DispatcherTest, DispatchMessageMultipleReceiversSecond) {
  const std::string sent_message{"World"};
  SimpleReceiver receiver1;
  SimpleReceiver receiver2;
  ReceiverMap receiver_map{{IsHello, receiver1}, {IsWorld, receiver2}};
  const Dispatcher dispatcher{receiver_map};
  auto reply = dispatcher.Dispatch(sent_message);
  EXPECT_TRUE(reply.has_value());
  EXPECT_EQ(reply.value(), "Received message");
  EXPECT_EQ(receiver1.received_message, "");
  EXPECT_EQ(receiver2.received_message, sent_message);
}

TEST_F(DispatcherTest, DispatchMessageUnhandled) {
  const std::string sent_message{"Unhandled"};
  SimpleReceiver receiver;
  ReceiverMap receiver_map{{IsHello, receiver}};
  const Dispatcher dispatcher{receiver_map};
  auto reply = dispatcher.Dispatch(sent_message);
  EXPECT_FALSE(reply.has_value());
  EXPECT_EQ(receiver.received_message, "");
}

}  // namespace
}  // namespace mcu
