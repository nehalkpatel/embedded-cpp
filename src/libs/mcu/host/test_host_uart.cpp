#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/host/host_peripheral_test_infra.hpp"
#include "libs/mcu/host/host_uart.hpp"
#include "libs/mcu/uart.hpp"

class HostUartTest : public mcu::test::HostPeripheralTest {
 protected:
  auto MakeReceiver(mcu::Transport& transport) -> mcu::Receiver& override {
    uart_ = std::make_unique<mcu::HostUart>("UART 1", transport);
    return *uart_;
  }

  // Emulator side of the UART protocol: Send stores into a loopback buffer,
  // Receive drains it.
  auto HandleRequest(std::string_view message)
      -> std::optional<std::string> override {
    auto request_result = mcu::Decode<mcu::UartEmulatorRequest>(message);
    if (!request_result) {
      return std::nullopt;  // Skip malformed messages
    }
    const auto& request = *request_result;
    mcu::UartEmulatorResponse response{
        .name = request.name,
        .data = {},
        .bytes_transferred = 0,
        .status = common::Error::kOk,
    };

    if (request.operation == mcu::OperationType::kSend) {
      uart_rx_buffer_.insert(uart_rx_buffer_.end(), request.data.begin(),
                             request.data.end());
      response.bytes_transferred = request.data.size();
    } else if (request.operation == mcu::OperationType::kReceive) {
      const size_t bytes_to_send{
          std::min(request.size, uart_rx_buffer_.size())};
      response.data = std::vector<std::byte>(
          uart_rx_buffer_.begin(),
          uart_rx_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_to_send));
      response.bytes_transferred = bytes_to_send;
      uart_rx_buffer_.erase(
          uart_rx_buffer_.begin(),
          uart_rx_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_to_send));
    }

    return mcu::Encode(response);
  }

  std::unique_ptr<mcu::HostUart> uart_;

 private:
  // Touched only from the emulator thread.
  std::vector<std::byte> uart_rx_buffer_;
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

TEST_F(HostUartTest, RxHandlerUnsolicitedData) {
  // Initialize UART
  const mcu::UartConfig config{};
  auto init_result = uart_->Init(config);
  ASSERT_TRUE(init_result);

  // Track received data via handler. The handler runs on the transport's
  // server thread while this thread reads the results, so both need
  // synchronisation -- a plain bool and vector here would be a data race.
  std::vector<std::byte> received_data{};
  std::mutex received_mutex{};
  std::atomic<bool> handler_called{false};

  // Register RxHandler
  auto handler_result =
      uart_->SetRxHandler([&received_data, &received_mutex, &handler_called](
                              const std::byte* data, size_t size) {
        {
          const std::lock_guard<std::mutex> lock(received_mutex);
          received_data.assign(data, data + size);
        }
        handler_called = true;
      });
  ASSERT_TRUE(handler_result);

  // Simulate emulator sending unsolicited data to device
  const std::vector<std::byte> test_data{std::byte{0xDE}, std::byte{0xAD},
                                         std::byte{0xBE}, std::byte{0xEF}};
  const mcu::UartEmulatorRequest unsolicited_request{
      .name = "UART 1",
      .operation = mcu::OperationType::kReceive,
      .data = test_data,
      .size = test_data.size(),
  };

  // Send unsolicited data directly via a fresh socket (simulating external
  // data arrival). Timeouts instead of a "connect time" sleep: a PAIR send
  // blocks until the pipe comes up, so SNDTIMEO bounds the wait.
  zmq::context_t unsolicited_context{1};
  zmq::socket_t unsolicited_socket{unsolicited_context, zmq::socket_type::pair};
  unsolicited_socket.set(zmq::sockopt::linger, 0);
  unsolicited_socket.set(zmq::sockopt::sndtimeo, 2000);
  unsolicited_socket.set(zmq::sockopt::rcvtimeo, 2000);
  unsolicited_socket.connect(device_endpoint_);

  const auto request_str = mcu::Encode(unsolicited_request);
  ASSERT_TRUE(
      unsolicited_socket.send(zmq::buffer(request_str), zmq::send_flags::none));

  // Wait for response (dispatcher should route and UART should respond).
  //
  // This recv is also the handler barrier, which is why no sleep follows it:
  // HostUart::Receive invokes rx_handler_ before it builds the ack, and the
  // server thread only replies once Dispatch has returned. A reply in hand
  // therefore happens-after the handler ran.
  zmq::message_t response_msg{};
  const auto recv_result{
      unsolicited_socket.recv(response_msg, zmq::recv_flags::none)};
  ASSERT_TRUE(recv_result.has_value());

  // Verify handler was called with correct data
  EXPECT_TRUE(handler_called);
  const std::lock_guard<std::mutex> lock(received_mutex);
  EXPECT_EQ(received_data, test_data);
}

TEST_F(HostUartTest, RxHandlerWithoutInit) {
  // Try to set handler before initialization
  auto result = uart_->SetRxHandler([](const std::byte*, size_t) {});
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error(), common::Error::kInvalidState);
}
