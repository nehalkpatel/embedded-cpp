#include <gtest/gtest.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
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
  static constexpr auto IsJson(std::string_view message) -> bool {
    return message.starts_with("{") && message.ends_with("}");
  }

  // Per-process endpoints. gtest_discover_tests gives every case its own
  // process, so a fixed path made `ctest -j` cases contend for one endpoint --
  // silently corrupting each other before EndpointLock, loudly after.
  static auto Endpoint(std::string_view role) -> std::string {
    return "ipc:///tmp/test_uart_" + std::string{role} + "_" +
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

    // Now create UART with transport
    uart_ = std::make_unique<mcu::HostUart>("UART 1", *device_transport_);

    // Add UART to receiver map (dispatcher holds reference, so this updates it)
    receiver_map_storage_.emplace_back(IsJson, std::ref(*uart_));

    // Wait for the condition rather than for a duration. On a PAIR socket a
    // send succeeds only once a pipe to the peer exists, so a successful probe
    // IS the readiness signal, and connect latency is absorbed by SNDTIMEO.
    // The emulator loop skips anything that fails to decode, so this non-JSON
    // probe is swallowed with no reply and needs no protocol support.
    ASSERT_TRUE(device_transport_->Send("probe"));
  }

  void TearDown() override {
    uart_.reset();
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
    unsolicited_context_.close();

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
    std::vector<std::byte> uart_rx_buffer;

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
            mcu::Decode<mcu::UartEmulatorRequest>(std::string{message_str});
        if (!request_result) {
          continue;  // Skip malformed messages
        }
        const auto& request = *request_result;
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

  const std::string device_emulator_endpoint_{Endpoint("device_emulator")};
  const std::string emulator_device_endpoint_{Endpoint("emulator_device")};
  mcu::ReceiverMap receiver_map_storage_;
  std::unique_ptr<mcu::Dispatcher> dispatcher_;
  std::unique_ptr<mcu::ZmqTransport> device_transport_;
  std::unique_ptr<mcu::HostUart> uart_;
  zmq::context_t emulator_context_{1};
  zmq::socket_t emulator_socket_{emulator_context_, zmq::socket_type::pair};
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

  // Track received data via handler. The handler runs on the transport's
  // server thread while this thread reads the results, so both need
  // synchronisation -- a plain bool and vector here were a data race
  // regardless of any sleep.
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
  // Timeouts instead of a "connect time" sleep. This socket had none, so its
  // send already blocked until the pipe came up -- the sleep was never what
  // made this work, it just hid an unbounded wait behind a bounded-looking one.
  unsolicited_socket.set(zmq::sockopt::linger, 0);
  unsolicited_socket.set(zmq::sockopt::sndtimeo, 2000);
  unsolicited_socket.set(zmq::sockopt::rcvtimeo, 2000);
  unsolicited_socket.connect(emulator_device_endpoint_);

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
