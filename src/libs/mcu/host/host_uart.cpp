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

auto HostUart::Send(std::span<const std::byte> data)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  const UartEmulatorRequest request{
      .name = name_,
      .operation = OperationType::kSend,
      .data = std::vector<std::byte>(data.begin(), data.end()),
  };
  return Transact<UartEmulatorResponse>(transport_, request)
      .transform([](const UartEmulatorResponse&) {});
}

auto HostUart::Receive(std::span<std::byte> buffer, uint32_t timeout_ms)
    -> std::expected<size_t, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  const UartEmulatorRequest request{
      .name = name_,
      .operation = OperationType::kReceive,
      .data = {},
      .size = buffer.size(),
      .timeout_ms = timeout_ms,
  };
  return Transact<UartEmulatorResponse>(transport_, request)
      .transform([buffer](const UartEmulatorResponse& response) {
        const size_t bytes_to_copy{
            std::min(buffer.size(), response.data.size())};
        std::copy_n(response.data.begin(), bytes_to_copy, buffer.begin());
        return bytes_to_copy;
      });
}

auto HostUart::SetRxHandler(std::function<void(const std::byte*, size_t)>
                                handler) -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  rx_handler_ = std::move(handler);
  return {};
}

// Messages arriving via the dispatcher are unsolicited requests from the
// emulator pushing data at the device; replies to this UART's own blocking
// operations return through Transport::Receive instead and never come here.
auto HostUart::Receive(std::string_view message)
    -> std::expected<std::string, common::Error> {
  auto request_result = Decode<UartEmulatorRequest>(message);
  if (!request_result || request_result->type != MessageType::kRequest) {
    return std::unexpected(common::Error::kInvalidArgument);
  }
  const auto& request = *request_result;

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
      .name = name_,
      .data = {},
      .bytes_transferred = request.data.size(),
      .status = common::Error::kOk,
  };
  return Encode(ack_response);
}

}  // namespace mcu
