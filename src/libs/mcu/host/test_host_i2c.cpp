#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/host/host_i2c.hpp"
#include "libs/mcu/host/host_peripheral_test_infra.hpp"
#include "libs/mcu/i2c.hpp"

class HostI2CTest : public mcu::test::HostPeripheralTest {
 protected:
  auto MakeReceiver(mcu::Transport& transport) -> mcu::Receiver& override {
    i2c_ = std::make_unique<mcu::HostI2CController>("I2C 1", transport);
    return *i2c_;
  }

  // Emulator side of the I2C protocol: one loopback buffer per device address.
  auto HandleRequest(std::string_view message)
      -> std::optional<std::string> override {
    auto request_result = mcu::Decode<mcu::I2CEmulatorRequest>(message);
    if (!request_result) {
      return std::nullopt;  // Skip malformed messages
    }
    const auto& request = *request_result;
    mcu::I2CEmulatorResponse response{
        .name = request.name,
        .address = request.address,
        .data = {},
        .bytes_transferred = 0,
        .status = common::Error::kOk,
    };

    if (request.operation == mcu::OperationType::kSend) {
      device_buffers_[request.address] = request.data;
      response.bytes_transferred = request.data.size();
    } else if (request.operation == mcu::OperationType::kReceive) {
      if (device_buffers_.contains(request.address)) {
        const auto& buffer = device_buffers_[request.address];
        const size_t bytes_to_send{std::min(request.size, buffer.size())};
        response.data = std::vector<std::byte>(
            buffer.begin(),
            buffer.begin() + static_cast<std::ptrdiff_t>(bytes_to_send));
        response.bytes_transferred = bytes_to_send;
      }
    }

    return mcu::Encode(response);
  }

  std::unique_ptr<mcu::HostI2CController> i2c_;

 private:
  // Touched only from the emulator thread.
  std::map<uint16_t, std::vector<std::byte>> device_buffers_;
};

TEST_F(HostI2CTest, SendData) {
  const uint16_t device_address{0x42};
  const std::array<std::byte, 4> send_data{std::byte{0xDE}, std::byte{0xAD},
                                           std::byte{0xBE}, std::byte{0xEF}};

  auto result = i2c_->SendData(device_address, send_data);
  EXPECT_TRUE(result);
}

TEST_F(HostI2CTest, SendReceiveData) {
  const uint16_t device_address{0x50};
  const std::array<std::byte, 5> send_data{std::byte{0x01}, std::byte{0x02},
                                           std::byte{0x03}, std::byte{0x04},
                                           std::byte{0x05}};

  // Send data to device
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  // Receive data back from same device (caller-provided buffer)
  std::array<std::byte, 5> recv_buffer{};
  auto recv_result = i2c_->ReceiveData(device_address, recv_buffer);
  ASSERT_TRUE(recv_result);

  const size_t bytes_received = recv_result.value();
  EXPECT_EQ(bytes_received, send_data.size());

  // Compare received data with sent data
  EXPECT_TRUE(std::equal(
      recv_buffer.begin(),
      recv_buffer.begin() + static_cast<std::ptrdiff_t>(bytes_received),
      send_data.begin(), send_data.end()));
}

TEST_F(HostI2CTest, MultipleAddresses) {
  const uint16_t address1{0x50};
  const uint16_t address2{0x51};
  const std::array<std::byte, 3> data1{std::byte{0xAA}, std::byte{0xBB},
                                       std::byte{0xCC}};
  const std::array<std::byte, 4> data2{std::byte{0x11}, std::byte{0x22},
                                       std::byte{0x33}, std::byte{0x44}};

  // Send to first address
  auto send1_result = i2c_->SendData(address1, data1);
  ASSERT_TRUE(send1_result);

  // Send to second address
  auto send2_result = i2c_->SendData(address2, data2);
  ASSERT_TRUE(send2_result);

  // Receive from first address
  std::array<std::byte, 3> recv1_buffer{};
  auto recv1_result = i2c_->ReceiveData(address1, recv1_buffer);
  ASSERT_TRUE(recv1_result);
  EXPECT_EQ(recv1_result.value(), data1.size());
  EXPECT_TRUE(std::equal(recv1_buffer.begin(), recv1_buffer.end(),
                         data1.begin(), data1.end()));

  // Receive from second address
  std::array<std::byte, 4> recv2_buffer{};
  auto recv2_result = i2c_->ReceiveData(address2, recv2_buffer);
  ASSERT_TRUE(recv2_result);
  EXPECT_EQ(recv2_result.value(), data2.size());
  EXPECT_TRUE(std::equal(recv2_buffer.begin(), recv2_buffer.end(),
                         data2.begin(), data2.end()));
}

TEST_F(HostI2CTest, ReceiveWithoutSend) {
  const uint16_t device_address{0x60};

  // Try to receive from device that has no data
  std::array<std::byte, 10> recv_buffer{};
  auto result = i2c_->ReceiveData(device_address, recv_buffer);
  ASSERT_TRUE(result);

  // Should return 0 bytes received
  EXPECT_EQ(result.value(), 0);
}

TEST_F(HostI2CTest, ReceivePartialData) {
  const uint16_t device_address{0x70};
  const std::array<std::byte, 10> send_data{
      std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
      std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}, std::byte{9}};

  // Send 10 bytes
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  // Request only 5 bytes (buffer size limits the receive)
  std::array<std::byte, 5> recv_buffer{};
  auto recv_result = i2c_->ReceiveData(device_address, recv_buffer);
  ASSERT_TRUE(recv_result);

  const size_t bytes_received = recv_result.value();
  EXPECT_EQ(bytes_received, 5);

  // Should receive first 5 bytes
  EXPECT_TRUE(std::equal(recv_buffer.begin(), recv_buffer.end(),
                         send_data.begin(), send_data.begin() + 5));
}
