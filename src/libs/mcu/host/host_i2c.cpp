#include "host_i2c.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "libs/common/error.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/i2c.hpp"

namespace mcu {

auto HostI2CController::SendData(uint16_t address,
                                 std::span<const uint8_t> data)
    -> std::expected<void, common::Error> {
  const I2CEmulatorRequest request{
      .type = MessageType::kRequest,
      .object = ObjectType::kI2C,
      .name = name_,
      .operation = OperationType::kSend,
      .address = address,
      .data = std::vector<uint8_t>(data.begin(), data.end()),
      .size = 0,
  };

  auto send_result = transport_.Send(Encode(request));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  auto receive_result = transport_.Receive();
  if (!receive_result) {
    return std::unexpected(receive_result.error());
  }

  const auto response = Decode<I2CEmulatorResponse>(receive_result.value());
  if (response.status != common::Error::kOk) {
    return std::unexpected(response.status);
  }

  return {};
}

auto HostI2CController::ReceiveData(uint16_t address, size_t size)
    -> std::expected<std::span<uint8_t>, common::Error> {
  const I2CEmulatorRequest request{
      .type = MessageType::kRequest,
      .object = ObjectType::kI2C,
      .name = name_,
      .operation = OperationType::kReceive,
      .address = address,
      .data = {},
      .size = size,
  };

  auto send_result = transport_.Send(Encode(request));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  auto receive_result = transport_.Receive();
  if (!receive_result) {
    return std::unexpected(receive_result.error());
  }

  const auto response = Decode<I2CEmulatorResponse>(receive_result.value());
  if (response.status != common::Error::kOk) {
    return std::unexpected(response.status);
  }

  // Store received data in buffer for this address
  auto& buffer = data_buffers_[address];
  const size_t bytes_to_copy{std::min(response.data.size(), buffer.size())};
  std::copy_n(response.data.begin(), bytes_to_copy, buffer.begin());

  return std::span<uint8_t>{buffer.data(), bytes_to_copy};
}

auto HostI2CController::SendDataInterrupt(
    uint16_t address, std::span<const uint8_t> data,
    void (*callback)(std::expected<void, common::Error>))
    -> std::expected<void, common::Error> {
  callback(SendData(address, data));
  return {};
}

auto HostI2CController::ReceiveDataInterrupt(
    uint16_t address, size_t size,
    void (*callback)(std::expected<std::span<uint8_t>, common::Error>))
    -> std::expected<void, common::Error> {
  callback(ReceiveData(address, size));
  return {};
}

auto HostI2CController::SendDataDma(
    uint16_t address, std::span<const uint8_t> data,
    void (*callback)(std::expected<void, common::Error>))
    -> std::expected<void, common::Error> {
  callback(SendData(address, data));
  return {};
}

auto HostI2CController::ReceiveDataDma(
    uint16_t address, size_t size,
    void (*callback)(std::expected<std::span<uint8_t>, common::Error>))
    -> std::expected<void, common::Error> {
  callback(ReceiveData(address, size));
  return {};
}

auto HostI2CController::Receive(const std::string_view& message)
    -> std::expected<std::string, common::Error> {
  static_cast<void>(message);
  return std::unexpected(common::Error::kUnhandled);
}

}  // namespace mcu
