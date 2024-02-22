#include <gtest/gtest.h>

#include "message_dispatcher.hpp"
#include "message_receiver.hpp"

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

class MessageDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  class SimpleReceiver : public MessageReceiver {
   public:
    void Receive(const std::string_view& message) override {
      received_message = message;
    }
    std::string_view received_message{};
  };
};

TEST_F(MessageDispatcherTest, DispatchMessage) {
  const std::string sent_message{"Hello"};
  SimpleReceiver receiver;
  ReceiverMap receiver_map = {{AcceptAll, receiver}};
  MessageDispatcher dispatcher{receiver_map};
  dispatcher.DispatchMessage(sent_message);
  EXPECT_EQ(receiver.received_message, sent_message);
}

TEST_F(MessageDispatcherTest, DispatchMessageReject) {
  const std::string sent_message{"Hello"};
  SimpleReceiver receiver;
  ReceiverMap receiver_map = {{RejectAll, receiver}};
  MessageDispatcher dispatcher{receiver_map};
  dispatcher.DispatchMessage(sent_message);
  EXPECT_EQ(receiver.received_message, "");
}

TEST_F(MessageDispatcherTest, DispatchMessageMultipleReceivers) {
  const std::string sent_message{"Hello"};
  SimpleReceiver receiver1;
  SimpleReceiver receiver2;
  ReceiverMap receiver_map = {{IsHello, receiver1}, {IsWorld, receiver2}};
  MessageDispatcher dispatcher{receiver_map};
  dispatcher.DispatchMessage(sent_message);
  EXPECT_EQ(receiver1.received_message, sent_message);
  EXPECT_EQ(receiver2.received_message, "");
}

TEST_F(MessageDispatcherTest, DispatchMessageMultipleReceiversSecond) {
  const std::string sent_message{"World"};
  SimpleReceiver receiver1;
  SimpleReceiver receiver2;
  ReceiverMap receiver_map = {{IsHello, receiver1}, {IsWorld, receiver2}};
  MessageDispatcher dispatcher{receiver_map};
  dispatcher.DispatchMessage(sent_message);
  EXPECT_EQ(receiver1.received_message, "");
  EXPECT_EQ(receiver2.received_message, sent_message);
}

}  // namespace
}  // namespace mcu
