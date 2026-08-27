#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
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

  // Per-process endpoints. gtest_discover_tests gives every case its own
  // process, so a fixed path made `ctest -j` cases contend for one endpoint --
  // silently corrupting each other before EndpointLock, loudly after.
  static auto Endpoint(std::string_view role) -> std::string {
    return "ipc:///tmp/test_i2c_" + std::string{role} + "_" +
           std::to_string(::getpid()) + ".ipc";
  }

  void SetUp() override {
    // Bind on the test thread, before the emulator thread exists, so the
    // transport's connect() below happens-after the bind by thread creation
    // alone. This replaces a 100ms sleep that only made the race unlikely.
    emulator_socket_.set(zmq::sockopt::linger, 0);
    emulator_socket_.bind(device_emulator_endpoint_);

    emulator_running_ = true;
    emulator_thread_ = std::thread{[this]() { EmulatorLoop(); }};

    // Create dispatcher with empty receiver map (will update via reference
    // later)
    dispatcher_ = std::make_unique<mcu::Dispatcher>(receiver_map_storage_);

    // Create transport. Assert rather than value_or(nullptr): Create can fail,
    // and a null transport is dereferenced two lines down.
    auto transport_result = mcu::ZmqTransport::Create(
        device_emulator_endpoint_, emulator_device_endpoint_, *dispatcher_);
    ASSERT_TRUE(transport_result.has_value());
    device_transport_ = std::move(transport_result.value());

    // Now create I2C with transport
    i2c_ =
        std::make_unique<mcu::HostI2CController>("I2C 1", *device_transport_);

    // Add I2C to receiver map (dispatcher holds reference, so this updates it)
    receiver_map_storage_.emplace_back(IsJson, std::ref(*i2c_));

    // Wait for the condition rather than for a duration. On a PAIR socket a
    // send succeeds only once a pipe to the peer exists, so a successful probe
    // IS the readiness signal, and connect latency is absorbed by SNDTIMEO.
    // The emulator loop skips anything that fails to decode, so this non-JSON
    // probe is swallowed with no reply and needs no protocol support.
    ASSERT_TRUE(device_transport_->Send("probe"));
  }

  void TearDown() override {
    i2c_.reset();
    device_transport_.reset();
    dispatcher_.reset();

    // Stop and join before closing anything the thread is using: terminating a
    // context out from under a running loop throws ETERM inside it.
    emulator_running_ = false;
    if (emulator_thread_.joinable()) {
      emulator_thread_.join();
    }
    emulator_socket_.close();
    emulator_context_.close();

    // The transport never unlinks its own lock file -- doing so would reopen
    // the race it closes -- so per-process test endpoints would otherwise pile
    // up in /tmp, one pair per test case per run.
    std::error_code error{};
    for (const auto& endpoint :
         {device_emulator_endpoint_, emulator_device_endpoint_}) {
      const std::string path{
          endpoint.substr(std::string_view{"ipc://"}.size())};
      std::filesystem::remove(path, error);
      std::filesystem::remove(path + ".lock", error);
    }
  }

  void EmulatorLoop() {
    // Simulate I2C device buffers (address -> data)
    std::map<uint16_t, std::vector<std::byte>> i2c_device_buffers;

    try {
      zmq::socket_t& socket = emulator_socket_;

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

        auto request_result =
            mcu::Decode<mcu::I2CEmulatorRequest>(std::string{message_str});
        if (!request_result) {
          continue;  // Skip malformed messages
        }
        const auto& request = *request_result;
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
            response.data = std::vector<std::byte>(
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

  const std::string device_emulator_endpoint_{Endpoint("device_emulator")};
  const std::string emulator_device_endpoint_{Endpoint("emulator_device")};
  mcu::ReceiverMap receiver_map_storage_;
  std::unique_ptr<mcu::Dispatcher> dispatcher_;
  std::unique_ptr<mcu::ZmqTransport> device_transport_;
  std::unique_ptr<mcu::HostI2CController> i2c_;
  zmq::context_t emulator_context_{1};
  zmq::socket_t emulator_socket_{emulator_context_, zmq::socket_type::pair};
  std::thread emulator_thread_;
  std::atomic<bool> emulator_running_{false};
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

TEST_F(HostI2CTest, SendDataInterrupt) {
  const uint16_t device_address{0x42};
  const std::array<std::byte, 3> send_data{std::byte{0xAA}, std::byte{0xBB},
                                           std::byte{0xCC}};

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
  const std::array<std::byte, 4> send_data{std::byte{0x01}, std::byte{0x02},
                                           std::byte{0x03}, std::byte{0x04}};

  // First send data
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  bool callback_called{false};
  std::expected<size_t, common::Error> callback_result{
      std::unexpected(common::Error::kUnknown)};
  std::array<std::byte, 4> recv_buffer{};

  auto result = i2c_->ReceiveDataInterrupt(
      device_address, recv_buffer,
      [&callback_called,
       &callback_result](std::expected<size_t, common::Error> result) {
        callback_called = true;
        callback_result = result;
      });

  EXPECT_TRUE(result);
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(callback_result);

  const size_t bytes_received = callback_result.value();
  EXPECT_EQ(bytes_received, send_data.size());
  EXPECT_TRUE(std::equal(recv_buffer.begin(), recv_buffer.end(),
                         send_data.begin(), send_data.end()));
}

TEST_F(HostI2CTest, SendDataDma) {
  const uint16_t device_address{0x42};
  const std::array<std::byte, 3> send_data{std::byte{0xDE}, std::byte{0xAD},
                                           std::byte{0xBE}};

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
  const std::array<std::byte, 5> send_data{std::byte{0x10}, std::byte{0x20},
                                           std::byte{0x30}, std::byte{0x40},
                                           std::byte{0x50}};

  // First send data
  auto send_result = i2c_->SendData(device_address, send_data);
  ASSERT_TRUE(send_result);

  bool callback_called{false};
  std::expected<size_t, common::Error> callback_result{
      std::unexpected(common::Error::kUnknown)};
  std::array<std::byte, 5> recv_buffer{};

  auto result =
      i2c_->ReceiveDataDma(device_address, recv_buffer,
                           [&callback_called, &callback_result](
                               std::expected<size_t, common::Error> result) {
                             callback_called = true;
                             callback_result = result;
                           });

  EXPECT_TRUE(result);
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(callback_result);

  const size_t bytes_received = callback_result.value();
  EXPECT_EQ(bytes_received, send_data.size());
  EXPECT_TRUE(std::equal(recv_buffer.begin(), recv_buffer.end(),
                         send_data.begin(), send_data.end()));
}
