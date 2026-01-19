#include <gtest/gtest.h>

#include "emulator_message_json_encoder.hpp"
#include "host_emulator_messages.hpp"

namespace mcu {
namespace {

TEST(EmulatorMessageJsonEncoderTest, EncodePinEmulatorRequest) {
  const PinEmulatorRequest request{.type = MessageType::kRequest,
                                   .object = ObjectType::kPin,
                                   .name = "PA0",
                                   .operation = OperationType::kSet,
                                   .state = PinState::kHigh};
  const std::string expected_json{
      R"({"name":"PA0","object":"Pin","operation":"Set","state":"High","type":"Request"})"};
  EXPECT_EQ(Encode(request), expected_json);
}

TEST(EmulatorMessageJsonEncoderTest, DecodePinEmulatorRequest) {
  const std::string json{
      R"({"name":"PA0","object":"Pin","operation":"Set","state":"High","type":"Request"})"};
  const PinEmulatorRequest expected_request{.type = MessageType::kRequest,
                                            .object = ObjectType::kPin,
                                            .name = "PA0",
                                            .operation = OperationType::kSet,
                                            .state = PinState::kHigh};
  auto result = Decode<PinEmulatorRequest>(json);
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, expected_request);
}

TEST(EmulatorMessageJsonEncoderTest, EncodeDecodePinEmulatorRequest) {
  const PinEmulatorRequest request{.type = MessageType::kRequest,
                                   .object = ObjectType::kPin,
                                   .name = "PA0",
                                   .operation = OperationType::kSet,
                                   .state = PinState::kHigh};
  const auto json{Encode(request)};
  auto decoded_request{Decode<PinEmulatorRequest>(json)};
  ASSERT_TRUE(decoded_request);
  EXPECT_EQ(*decoded_request, request);
}

TEST(EmulatorMessageJsonEncoderTest, DecodeInvalidJson) {
  const std::string invalid_json{"not valid json"};
  auto result = Decode<PinEmulatorRequest>(invalid_json);
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidArgument);
}

}  // namespace
}  // namespace mcu
