#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "emulator_message_json_encoder.hpp"
#include "host_emulator_messages.hpp"

namespace mcu {
namespace {

// One representative value per message type, with every optional field
// populated so the round trip exercises the whole schema — including the
// std::byte vectors, which go through the custom adl_serializer.
template <typename T>
auto Sample() -> T;

const std::vector<std::byte> kSampleData{std::byte{0xDE}, std::byte{0xAD},
                                         std::byte{0xBE}, std::byte{0xEF}};

template <>
auto Sample<PinEmulatorRequest>() -> PinEmulatorRequest {
  return {.name = "PA0",
          .operation = OperationType::kSet,
          .state = PinState::kHigh};
}

template <>
auto Sample<PinEmulatorResponse>() -> PinEmulatorResponse {
  return {.name = "PA0", .state = PinState::kLow, .status = common::Error::kOk};
}

template <>
auto Sample<UartEmulatorRequest>() -> UartEmulatorRequest {
  return {.name = "UART 1",
          .operation = OperationType::kSend,
          .data = kSampleData,
          .size = 16,
          .timeout_ms = 250};
}

template <>
auto Sample<UartEmulatorResponse>() -> UartEmulatorResponse {
  return {.name = "UART 1",
          .data = kSampleData,
          .bytes_transferred = kSampleData.size(),
          .status = common::Error::kTimeout};
}

template <>
auto Sample<I2CEmulatorRequest>() -> I2CEmulatorRequest {
  return {.name = "I2C 1",
          .operation = OperationType::kReceive,
          .address = 0x50,
          .data = kSampleData,
          .size = 4};
}

template <>
auto Sample<I2CEmulatorResponse>() -> I2CEmulatorResponse {
  return {.name = "I2C 1",
          .address = 0x50,
          .data = kSampleData,
          .bytes_transferred = kSampleData.size(),
          .status = common::Error::kOk};
}

template <typename T>
class MessageRoundTripTest : public ::testing::Test {};

using AllMessageTypes =
    ::testing::Types<PinEmulatorRequest, PinEmulatorResponse,
                     UartEmulatorRequest, UartEmulatorResponse,
                     I2CEmulatorRequest, I2CEmulatorResponse>;
TYPED_TEST_SUITE(MessageRoundTripTest, AllMessageTypes);

TYPED_TEST(MessageRoundTripTest, EncodeDecodeRoundTrips) {
  const TypeParam original = Sample<TypeParam>();
  auto decoded = Decode<TypeParam>(Encode(original));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(*decoded, original);
}

// Pins the exact wire format the Python emulator parses; a change here is a
// protocol change, not a refactor.
TEST(EmulatorMessageJsonEncoderTest, PinRequestWireFormat) {
  const std::string wire_json{
      R"({"name":"PA0","object":"Pin","operation":"Set","state":"High","type":"Request"})"};

  EXPECT_EQ(Encode(Sample<PinEmulatorRequest>()), wire_json);

  auto decoded = Decode<PinEmulatorRequest>(wire_json);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(*decoded, Sample<PinEmulatorRequest>());
}

TEST(EmulatorMessageJsonEncoderTest, DecodeRejectsInvalidJson) {
  auto result = Decode<PinEmulatorRequest>("not valid json");
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidArgument);
}

// Valid JSON, wrong shape: exercises the type-mismatch path (the narrow catch
// around get<T>), not the parser.
TEST(EmulatorMessageJsonEncoderTest, DecodeRejectsWrongStructure) {
  auto result = Decode<PinEmulatorRequest>(R"({"unrelated": 42})");
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidArgument);
}

}  // namespace
}  // namespace mcu
