#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <thread>
#include <vector>

#include "libs/mcu/host/dispatcher.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/host/host_uart.hpp"
#include "libs/mcu/host/zmq_transport.hpp"
#include "libs/mcu/uart.hpp"

class HostUartTest : public ::testing::Test {
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
    device_transport_ =
        mcu::ZmqTransport::Create("ipc:///tmp/test_uart_device_emulator.ipc",
                                  "ipc:///tmp/test_uart_emulator_device.ipc",
                                  *dispatcher_)
            .value_or(nullptr);

    // Now create UART with transport
    uart_ = std::make_unique<mcu::HostUart>("UART 1", *device_transport_);

    // Add UART to receiver map (dispatcher holds reference, so this updates it)
    receiver_map_storage_.emplace_back(IsJson, std::ref(*uart_));

    // Give transport time to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void TearDown() override {
    uart_.reset();
    device_transport_.reset();
    dispatcher_.reset();

    emulator_running_ = false;
    if (emulator_thread_.joinable()) {
      emulator_context_.shutdown();
      emulator_context_.close();
      unsolicited_context_.shutdown();
      unsolicited_context_.close();
      emulator_thread_.join();
    }
  }

  void EmulatorLoop() {
    std::vector<std::byte> uart_rx_buffer;

    try {
      zmq::socket_t socket{emulator_context_, zmq::socket_type::pair};
      socket.bind("ipc:///tmp/test_uart_device_emulator.ipc");

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
            mcu::Decode<mcu::UartEmulatorRequest>(std::string{message_str});
        mcu::UartEmulatorResponse response{
            .type = mcu::MessageType::kResponse,
            .object = mcu::ObjectType::kUart,
            .name = request.name,
            .data = {},
            .bytes_transferred = 0,
            .status = common::Error::kOk,
        };

        if (request.operation == mcu::OperationType::kSend) {
          // Device sent data - store in our buffer
          uart_rx_buffer.insert(uart_rx_buffer.end(), request.data.begin(),
                                request.data.end());
          response.bytes_transferred = request.data.size();
        } else if (request.operation == mcu::OperationType::kReceive) {
          // Device wants to receive data - send from our buffer
          const size_t bytes_to_send{
              std::min(request.size, uart_rx_buffer.size())};
          response.data = std::vector<std::byte>(
              uart_rx_buffer.begin(),
              uart_rx_buffer.begin() +
                  static_cast<std::ptrdiff_t>(bytes_to_send));
          response.bytes_transferred = bytes_to_send;
          uart_rx_buffer.erase(uart_rx_buffer.begin(),
                               uart_rx_buffer.begin() +
                                   static_cast<std::ptrdiff_t>(bytes_to_send));
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

  mcu::ReceiverMap receiver_map_storage_;
  std::unique_ptr<mcu::Dispatcher> dispatcher_;
  std::unique_ptr<mcu::ZmqTransport> device_transport_;
  std::unique_ptr<mcu::HostUart> uart_;
  zmq::context_t emulator_context_{1};
  zmq::context_t unsolicited_context_{1};
  std::thread emulator_thread_;
  std::atomic<bool> emulator_running_{false};
};

TEST_F(HostUartTest, Init) {
  const mcu::UartConfig config{
      .baud_rate = 115200,
      .data_bits = mcu::UartConfig::DataBits::k8Bits,
      .parity = mcu::UartConfig::Parity::kNone,
      .stop_bits = mcu::UartConfig::StopBits::k1Bit,
      .flow_control = mcu::UartConfig::FlowControl::kNone,
  };

  auto result = uart_->Init(config);
  EXPECT_TRUE(result);
}

TEST_F(HostUartTest, SendReceiveBlocking) {
  // Initialize UART
  const mcu::UartConfig config{};
  auto init_result = uart_->Init(config);
  ASSERT_TRUE(init_result);

  // Send data
  const std::array<std::byte, 5> send_data{std::byte{0x01}, std::byte{0x02},
                                           std::byte{0x03}, std::byte{0x04},
                                           std::byte{0x05}};
  auto send_result = uart_->Send(send_data);
  EXPECT_TRUE(send_result);

  // Receive data back (emulator echoes to buffer)
  std::array<std::byte, 5> recv_buffer = {};
  auto recv_result = uart_->Receive(recv_buffer, 1000);
  ASSERT_TRUE(recv_result);
  EXPECT_EQ(recv_result.value(), 5);
  EXPECT_EQ(recv_buffer, send_data);
}

TEST_F(HostUartTest, SendWithoutInit) {
  const std::array<std::byte, 5> send_data{std::byte{0x01}, std::byte{0x02},
                                           std::byte{0x03}, std::byte{0x04},
                                           std::byte{0x05}};
  auto result = uart_->Send(send_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidState);
}

TEST_F(HostUartTest, ReceiveWithoutInit) {
  std::array<std::byte, 5> recv_buffer = {};
  auto result = uart_->Receive(recv_buffer, 1000);
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidState);
}

TEST_F(HostUartTest, IsBusy) {
  const mcu::UartConfig config{};
  auto init_result = uart_->Init(config);
  ASSERT_TRUE(init_result);

  EXPECT_FALSE(uart_->IsBusy());

  const std::array<std::byte, 5> send_data{std::byte{0x01}, std::byte{0x02},
                                           std::byte{0x03}, std::byte{0x04},
                                           std::byte{0x05}};
  std::ignore = uart_->Send(send_data);

  EXPECT_FALSE(uart_->IsBusy());  // Blocking operation completes immediately
}

TEST_F(HostUartTest, Available) {
  const mcu::UartConfig config{};
  auto init_result = uart_->Init(config);
  ASSERT_TRUE(init_result);

  // For host implementation, Available() always returns 0
  // (data is retrieved on-demand from emulator)
  EXPECT_EQ(uart_->Available(), 0);
}

TEST_F(HostUartTest, Flush) {
  const mcu::UartConfig config{};
  auto init_result = uart_->Init(config);
  ASSERT_TRUE(init_result);

  auto result = uart_->Flush();
  EXPECT_TRUE(result);
}

TEST_F(HostUartTest, RxHandlerUnsolicitedData) {
  // Initialize UART
  const mcu::UartConfig config{};
  auto init_result = uart_->Init(config);
  ASSERT_TRUE(init_result);

  // Track received data via handler
  std::vector<std::byte> received_data{};
  bool handler_called{false};

  // Register RxHandler
  auto handler_result = uart_->SetRxHandler(
      [&received_data, &handler_called](const std::byte* data, size_t size) {
        received_data.assign(data, data + size);
        handler_called = true;
      });
  ASSERT_TRUE(handler_result);

  // Simulate emulator sending unsolicited data to device
  const std::vector<std::byte> test_data{std::byte{0xDE}, std::byte{0xAD},
                                         std::byte{0xBE}, std::byte{0xEF}};
  const mcu::UartEmulatorRequest unsolicited_request{
      .type = mcu::MessageType::kRequest,
      .object = mcu::ObjectType::kUart,
      .name = "UART 1",
      .operation = mcu::OperationType::kReceive,
      .data = test_data,
      .size = test_data.size(),
      .timeout_ms = 0,
  };

  // Send unsolicited data directly via socket (simulating external data
  // arrival)
  zmq::socket_t unsolicited_socket{unsolicited_context_,
                                   zmq::socket_type::pair};
  unsolicited_socket.connect("ipc:///tmp/test_uart_emulator_device.ipc");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Connect time

  const auto request_str = mcu::Encode(unsolicited_request);
  unsolicited_socket.send(zmq::buffer(request_str), zmq::send_flags::none);

  // Wait for response (dispatcher should route and UART should respond)
  zmq::message_t response_msg{};
  const auto recv_result{
      unsolicited_socket.recv(response_msg, zmq::recv_flags::none)};
  ASSERT_TRUE(recv_result.has_value());

  // Give handler time to execute
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify handler was called with correct data
  EXPECT_TRUE(handler_called);
  EXPECT_EQ(received_data, test_data);
}

TEST_F(HostUartTest, RxHandlerWithoutInit) {
  // Try to set handler before initialization
  auto result = uart_->SetRxHandler([](const std::byte*, size_t) {});
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidState);
}
