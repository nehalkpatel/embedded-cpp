#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "libs/mcu/host/dispatcher.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/host/host_i2c.hpp"
#include "libs/mcu/host/zmq_transport.hpp"
#include "libs/mcu/i2c.hpp"

class HostI2CTest : public ::testing::Test {
 protected:
  static constexpr auto IsJson(const std::string_view& message) -> bool {
    return message.starts_with("{") && message.ends_with("}");
  }

  void SetUp() override {
    // Start emulator thread
    emulator_running_ = true;
    emulator_thread_ = std::thread{[this]() { EmulatorLoop(); }};

    // Give emulator time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create dispatcher with empty receiver map (will update via reference
    // later)
    dispatcher_ = std::make_unique<mcu::Dispatcher>(receiver_map_storage_);

    // Create transport
    device_transport_ = std::make_unique<mcu::ZmqTransport>(
        "ipc:///tmp/test_i2c_device_emulator.ipc",
        "ipc:///tmp/test_i2c_emulator_device.ipc", *dispatcher_);

    // Now create I2C with transport
    i2c_ =
        std::make_unique<mcu::HostI2CController>("I2C 1", *device_transport_);

    // Add I2C to receiver map (dispatcher holds reference, so this updates it)
    receiver_map_storage_.emplace_back(IsJson, std::ref(*i2c_));

    // Give transport time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    i2c_.reset();
    device_transport_.reset();
    dispatcher_.reset();

    emulator_running_ = false;
    if (emulator_thread_.joinable()) {
      emulator_context_.shutdown();
      emulator_context_.close();
      emulator_thread_.join();
    }
  }

  void EmulatorLoop() {
    // Simulate I2C device buffers (address -> data)
    std::map<uint16_t, std::vector<uint8_t>> i2c_device_buffers;

    try {
      zmq::socket_t socket{emulator_context_, zmq::socket_type::pair};
      socket.bind("ipc:///tmp/test_i2c_device_emulator.ipc");

      while (emulator_running_) {
        std::array<zmq::pollitem_t, 1> items = {
            {{.socket = static_cast<void*>(socket),
              .fd = 0,
              .events = ZMQ_POLLIN,
              .revents = 0}}};

        const int ret{
            zmq::poll(items.data(), 1, std::chrono::milliseconds{50})};

        if (ret == 0) {
          continue;  // Timeout
        }
        if (ret <= 0) {
          continue;
        }

        zmq::message_t message{};
        if (!socket.recv(message, zmq::recv_flags::none)) {
          continue;
        }

        const std::string_view message_str{
            static_cast<const char*>(message.data()), message.size()};

        const auto request =
            mcu::Decode<mcu::I2CEmulatorRequest>(std::string{message_str});
        mcu::I2CEmulatorResponse response{
            .type = mcu::MessageType::kResponse,
            .object = mcu::ObjectType::kI2C,
            .name = request.name,
            .address = request.address,
            .data = {},
            .bytes_transferred = 0,
            .status = common::Error::kOk,
        };

        if (request.operation == mcu::OperationType::kSend) {
          // Device sent data to I2C peripheral - store in device buffer
          i2c_device_buffers[request.address] = request.data;
          response.bytes_transferred = request.data.size();
        } else if (request.operation == mcu::OperationType::kReceive) {
          // Device wants to receive data from I2C peripheral
          if (i2c_device_buffers.contains(request.address)) {
            const auto& buffer = i2c_device_buffers[request.address];
            const size_t bytes_to_send{std::min(request.size, buffer.size())};
            response.data = std::vector<uint8_t>(
                buffer.begin(),
                buffer.begin() + static_cast<std::ptrdiff_t>(bytes_to_send));
            response.bytes_transferred = bytes_to_send;
          }
        }

        const auto response_str = mcu::Encode(response);
        socket.send(zmq::buffer(response_str), zmq::send_flags::none);
      }
    } catch (const zmq::error_t& e) {
      // Socket closed during shutdown, expected behavior
      if (e.num() != ETERM) {
        throw;
      }
    }
  }

  std::vector<
      std::pair<std::function<bool(const std::string_view&)>, mcu::Receiver&>>
      receiver_map_storage_;
  std::unique_ptr<mcu::Dispatcher> dispatcher_;
  std::unique_ptr<mcu::ZmqTransport> device_transport_;
  std::unique_ptr<mcu::HostI2CController> i2c_;
  zmq::context_t emulator_context_{1};
  std::thread emulator_thread_;
  std::atomic<bool> emulator_running_{false};
};

TEST_F(HostI2CTest, SendData) {
  const uint16_t device_address{0x42};
  const std::array<uint8_t, 4> send_data{0xDE, 0xAD, 0xBE, 0xEF};

  auto result = i2c_->SendData(device_address, send_data);
  EXPECT_TRUE(result);
}

TEST_F(HostI2CTest, SendReceiveData) {
  const uint16_t device_address{0x50};
  const std::array<uint8_t, 5> send_data{0x01, 0x02, 0x03, 0x04, 0x05};

  // Send data to device
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  // Receive data back from same device
  auto recv_result = i2c_->ReceiveData(device_address, send_data.size());
  ASSERT_TRUE(recv_result);

  const auto received_span = recv_result.value();
  EXPECT_EQ(received_span.size(), send_data.size());

  // Compare received data with sent data
  EXPECT_TRUE(std::equal(received_span.begin(), received_span.end(),
                         send_data.begin(), send_data.end()));
}

TEST_F(HostI2CTest, MultipleAddresses) {
  const uint16_t address1{0x50};
  const uint16_t address2{0x51};
  const std::array<uint8_t, 3> data1{0xAA, 0xBB, 0xCC};
  const std::array<uint8_t, 4> data2{0x11, 0x22, 0x33, 0x44};

  // Send to first address
  auto send1_result = i2c_->SendData(address1, data1);
  ASSERT_TRUE(send1_result);

  // Send to second address
  auto send2_result = i2c_->SendData(address2, data2);
  ASSERT_TRUE(send2_result);

  // Receive from first address
  auto recv1_result = i2c_->ReceiveData(address1, data1.size());
  ASSERT_TRUE(recv1_result);
  const auto received1_span = recv1_result.value();
  EXPECT_EQ(received1_span.size(), data1.size());
  EXPECT_TRUE(std::equal(received1_span.begin(), received1_span.end(),
                         data1.begin(), data1.end()));

  // Receive from second address
  auto recv2_result = i2c_->ReceiveData(address2, data2.size());
  ASSERT_TRUE(recv2_result);
  const auto received2_span = recv2_result.value();
  EXPECT_EQ(received2_span.size(), data2.size());
  EXPECT_TRUE(std::equal(received2_span.begin(), received2_span.end(),
                         data2.begin(), data2.end()));
}

TEST_F(HostI2CTest, ReceiveWithoutSend) {
  const uint16_t device_address{0x60};

  // Try to receive from device that has no data
  auto result = i2c_->ReceiveData(device_address, 10);
  ASSERT_TRUE(result);

  // Should return empty span
  const auto received_span = result.value();
  EXPECT_EQ(received_span.size(), 0);
}

TEST_F(HostI2CTest, ReceivePartialData) {
  const uint16_t device_address{0x70};
  const std::array<uint8_t, 10> send_data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  // Send 10 bytes
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  // Request only 5 bytes
  auto recv_result = i2c_->ReceiveData(device_address, 5);
  ASSERT_TRUE(recv_result);

  const auto received_span = recv_result.value();
  EXPECT_EQ(received_span.size(), 5);

  // Should receive first 5 bytes
  EXPECT_TRUE(std::equal(received_span.begin(), received_span.end(),
                         send_data.begin(), send_data.begin() + 5));
}

TEST_F(HostI2CTest, SendDataInterrupt) {
  const uint16_t device_address{0x42};
  const std::array<uint8_t, 3> send_data{0xAA, 0xBB, 0xCC};

  bool callback_called{false};
  std::expected<void, common::Error> callback_result{};

  auto result =
      i2c_->SendDataInterrupt(device_address, send_data,
                              [&callback_called, &callback_result](
                                  std::expected<void, common::Error> result) {
                                callback_called = true;
                                callback_result = result;
                              });

  EXPECT_TRUE(result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(HostI2CTest, ReceiveDataInterrupt) {
  const uint16_t device_address{0x50};
  const std::array<uint8_t, 4> send_data{0x01, 0x02, 0x03, 0x04};

  // First send data
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  bool callback_called{false};
  std::expected<std::span<uint8_t>, common::Error> callback_result{
      std::unexpected(common::Error::kUnknown)};

  auto result = i2c_->ReceiveDataInterrupt(
      device_address, send_data.size(),
      [&callback_called, &callback_result](
          std::expected<std::span<uint8_t>, common::Error> result) {
        callback_called = true;
        callback_result = result;
      });

  EXPECT_TRUE(result);
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(callback_result);

  const auto received_span = callback_result.value();
  EXPECT_EQ(received_span.size(), send_data.size());
  EXPECT_TRUE(std::equal(received_span.begin(), received_span.end(),
                         send_data.begin(), send_data.end()));
}

TEST_F(HostI2CTest, SendDataDma) {
  const uint16_t device_address{0x42};
  const std::array<uint8_t, 3> send_data{0xDE, 0xAD, 0xBE};

  bool callback_called{false};
  std::expected<void, common::Error> callback_result{};

  auto result =
      i2c_->SendDataDma(device_address, send_data,
                        [&callback_called, &callback_result](
                            std::expected<void, common::Error> result) {
                          callback_called = true;
                          callback_result = result;
                        });

  EXPECT_TRUE(result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(callback_result);
}

TEST_F(HostI2CTest, ReceiveDataDma) {
  const uint16_t device_address{0x55};
  const std::array<uint8_t, 5> send_data{0x10, 0x20, 0x30, 0x40, 0x50};

  // First send data
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  bool callback_called{false};
  std::expected<std::span<uint8_t>, common::Error> callback_result{
      std::unexpected(common::Error::kUnknown)};

  auto result = i2c_->ReceiveDataDma(
      device_address, send_data.size(),
      [&callback_called, &callback_result](
          std::expected<std::span<uint8_t>, common::Error> result) {
        callback_called = true;
        callback_result = result;
      });

  EXPECT_TRUE(result);
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(callback_result);

  const auto received_span = callback_result.value();
  EXPECT_EQ(received_span.size(), send_data.size());
  EXPECT_TRUE(std::equal(received_span.begin(), received_span.end(),
                         send_data.begin(), send_data.end()));
}
