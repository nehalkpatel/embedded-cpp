#include <gtest/gtest.h>

#include <array>
#include <expected>
#include <functional>
#include <string>

#include "dispatcher.hpp"
#include "libs/common/error.hpp"
#include "receiver.hpp"

namespace mcu {
namespace {

// Mirrors the real receivers' contract: accept only messages addressed to it
// (an unexpected return means "not mine, keep looking"), and record what it
// accepted.
class NamedReceiver : public Receiver {
 public:
  explicit NamedReceiver(std::string name) : name_{std::move(name)} {}

  auto Receive(std::string_view message)
      -> std::expected<std::string, common::Error> override {
    if (message != name_) {
      return std::unexpected(common::Error::kInvalidArgument);
    }
    received_message = std::string{message};
    return {"Received message"};
  }

  std::string received_message;

 private:
  std::string name_;
};

struct RoutingCase {
  std::string message;
  size_t expected_receiver;
};

class DispatcherRoutingTest : public ::testing::TestWithParam<RoutingCase> {};

TEST_P(DispatcherRoutingTest, DeliversToTheReceiverThatClaimsTheMessage) {
  const auto& [message, expected_receiver] = GetParam();
  std::array<NamedReceiver, 2> receivers{NamedReceiver{"Hello"},
                                         NamedReceiver{"World"}};
  const ReceiverMap receiver_map{std::ref(receivers[0]),
                                 std::ref(receivers[1])};
  const Dispatcher dispatcher{receiver_map};

  auto reply = dispatcher.Dispatch(message);

  ASSERT_TRUE(reply.has_value());
  EXPECT_EQ(reply.value(), "Received message");
  for (size_t index = 0; index < receivers.size(); ++index) {
    const auto& expected = index == expected_receiver ? message : std::string{};
    EXPECT_EQ(receivers.at(index).received_message, expected);
  }
}

INSTANTIATE_TEST_SUITE_P(EachReceiver, DispatcherRoutingTest,
                         ::testing::Values(RoutingCase{"Hello", 0},
                                           RoutingCase{"World", 1}));

TEST(DispatcherTest, ReportsUnhandledWhenNoReceiverClaimsTheMessage) {
  NamedReceiver receiver{"Hello"};
  const ReceiverMap receiver_map{std::ref(receiver)};
  const Dispatcher dispatcher{receiver_map};

  auto reply = dispatcher.Dispatch("Unhandled");

  ASSERT_FALSE(reply.has_value());
  EXPECT_EQ(reply.error(), common::Error::kUnhandled);
  EXPECT_EQ(receiver.received_message, "");
}

}  // namespace
}  // namespace mcu
