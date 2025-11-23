#include "libs/mcu/host/host_uart.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "libs/common/error.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/uart.hpp"

namespace mcu {

auto HostUart::Init(const UartConfig& config)
    -> std::expected<void, common::Error> {
  if (initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  config_ = config;
  initialized_ = true;
  return {};
}

auto HostUart::Send(std::span<const uint8_t> data)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  const UartEmulatorRequest request{
      .type = MessageType::kRequest,
      .object = ObjectType::kUart,
      .name = name_,
      .operation = OperationType::kSend,
      .data = std::vector<uint8_t>(data.begin(), data.end()),
      .size = 0,
      .timeout_ms = 0,
  };

  auto send_result = transport_.Send(Encode(request));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  auto response_str = transport_.Receive();
  if (!response_str) {
    return std::unexpected(response_str.error());
  }

  const auto response = Decode<UartEmulatorResponse>(response_str.value());
  if (response.status != common::Error::kOk) {
    return std::unexpected(response.status);
  }

  return {};
}

auto HostUart::Receive(std::span<uint8_t> buffer, uint32_t timeout_ms)
    -> std::expected<size_t, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  const UartEmulatorRequest request{
      .type = MessageType::kRequest,
      .object = ObjectType::kUart,
      .name = name_,
      .operation = OperationType::kReceive,
      .data = {},
      .size = buffer.size(),
      .timeout_ms = timeout_ms,
  };

  auto send_result = transport_.Send(Encode(request));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  auto response_str = transport_.Receive();
  if (!response_str) {
    return std::unexpected(response_str.error());
  }

  const auto response = Decode<UartEmulatorResponse>(response_str.value());
  if (response.status != common::Error::kOk) {
    return std::unexpected(response.status);
  }

  // Copy received data to buffer
  const size_t bytes_to_copy =
      std::min(buffer.size(), response.data.size());
  std::copy_n(response.data.begin(), bytes_to_copy, buffer.begin());

  return bytes_to_copy;
}

auto HostUart::SendAsync(
    std::span<const uint8_t> data,
    std::function<void(std::expected<void, common::Error>)> callback)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  busy_ = true;
  send_callback_ = std::move(callback);

  const UartEmulatorRequest request{
      .type = MessageType::kRequest,
      .object = ObjectType::kUart,
      .name = name_,
      .operation = OperationType::kSend,
      .data = std::vector<uint8_t>(data.begin(), data.end()),
      .size = 0,
      .timeout_ms = 0,
  };

  auto result = transport_.Send(Encode(request));
  if (!result) {
    busy_ = false;
    send_callback_ = {};
    return std::unexpected(result.error());
  }

  // Response will come asynchronously via Receive() method
  return {};
}

auto HostUart::ReceiveAsync(
    std::span<uint8_t> buffer,
    std::function<void(std::expected<size_t, common::Error>)> callback)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  busy_ = true;
  receive_callback_ = std::move(callback);
  receive_buffer_.resize(buffer.size());

  const UartEmulatorRequest request{
      .type = MessageType::kRequest,
      .object = ObjectType::kUart,
      .name = name_,
      .operation = OperationType::kReceive,
      .data = {},
      .size = buffer.size(),
      .timeout_ms = 0,
  };

  auto result = transport_.Send(Encode(request));
  if (!result) {
    busy_ = false;
    receive_callback_ = {};
    receive_buffer_.clear();
    return std::unexpected(result.error());
  }

  // Response will come asynchronously via Receive() method
  // The buffer span will be filled when response arrives
  return {};
}

auto HostUart::IsBusy() -> bool { return busy_; }

auto HostUart::Available() -> size_t {
  // For host implementation, we don't maintain a receive buffer
  // Always return 0 (data is retrieved on-demand from emulator)
  return 0;
}

auto HostUart::Flush() -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  // For host implementation, no buffering occurs
  // Nothing to flush
  return {};
}

auto HostUart::SetRxHandler(std::function<void(const uint8_t*, size_t)> handler)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  rx_handler_ = std::move(handler);
  return {};
}

auto HostUart::Receive(const std::string_view& message)
    -> std::expected<std::string, common::Error> {
  // First, check if this is a request (unsolicited data) or response
  const auto json_msg = nlohmann::json::parse(message);

  // Handle unsolicited incoming data from emulator (Request type)
  if (json_msg["type"] == MessageType::kRequest) {
    const auto request = Decode<UartEmulatorRequest>(message);

    // Verify this message is for us
    if (request.name != name_) {
      return std::unexpected(common::Error::kInvalidArgument);
    }

    // Only handle "Receive" operation (emulator pushing data to device)
    if (request.operation != OperationType::kReceive) {
      return std::unexpected(common::Error::kInvalidOperation);
    }

    // Invoke RxHandler if registered
    if (rx_handler_ && !request.data.empty()) {
      rx_handler_(request.data.data(), request.data.size());
    }

    // Send acknowledgment response
    const UartEmulatorResponse ack_response{
        .type = MessageType::kResponse,
        .object = ObjectType::kUart,
        .name = name_,
        .data = {},
        .bytes_transferred = request.data.size(),
        .status = common::Error::kOk,
    };

    return Encode(ack_response);
  }

  // Handle async operation responses
  const auto response = Decode<UartEmulatorResponse>(message);

  // Verify this message is for us
  if (response.name != name_) {
    return std::unexpected(common::Error::kInvalidArgument);
  }

  if (!busy_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  // Handle async send response
  if (send_callback_) {
    auto callback = std::move(send_callback_);
    send_callback_ = {};
    busy_ = false;

    if (response.status != common::Error::kOk) {
      callback(std::unexpected(response.status));
    } else {
      callback({});
    }
    return std::string{};  // Message consumed
  }

  // Handle async receive response
  if (receive_callback_) {
    auto callback = std::move(receive_callback_);
    receive_callback_ = {};
    busy_ = false;

    if (response.status != common::Error::kOk) {
      receive_buffer_.clear();
      callback(std::unexpected(response.status));
    } else {
      // Copy received data to buffer (stored for the callback)
      const size_t bytes_received = response.bytes_transferred;
      receive_buffer_.resize(bytes_received);
      std::copy_n(response.data.begin(), bytes_received,
                  receive_buffer_.begin());
      callback(bytes_received);
    }
    return std::string{};  // Message consumed
  }

  // No callback registered - unexpected state
  busy_ = false;
  return std::unexpected(common::Error::kInvalidState);
}

}  // namespace mcu
