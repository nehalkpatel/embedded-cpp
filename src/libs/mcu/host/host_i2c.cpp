#include "host_i2c.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "libs/common/error.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/i2c.hpp"

namespace mcu {
auto HostI2CController::SendData(uint16_t address,
                                 std::span<const uint8_t> data)
    -> std::expected<void, int> {
  std::ranges::copy(data, data_buffers_[address].begin());
  return {};
}

auto HostI2CController::ReceiveData(uint16_t address, size_t size)
    -> std::expected<std::span<uint8_t>, int> {
  return std::span<uint8_t>{data_buffers_[address].data(),
                            std::min(size, data_buffers_[address].size())};
}

auto HostI2CController::SendDataInterrupt(
    uint16_t address, std::span<const uint8_t> data,
    void (*callback)(std::expected<void, int>)) -> std::expected<void, int> {
  callback(SendData(address, data));
  return {};
}

auto HostI2CController::ReceiveDataInterrupt(
    uint16_t address, size_t size,
    void (*callback)(std::expected<std::span<uint8_t>, int>))
    -> std::expected<void, int> {
  callback(ReceiveData(address, size));
  return {};
}

auto HostI2CController::SendDataDma(
    uint16_t address, std::span<const uint8_t> data,
    void (*callback)(std::expected<void, int>)) -> std::expected<void, int> {
  callback(SendData(address, data));
  return {};
}

auto HostI2CController::ReceiveDataDma(
    uint16_t address, size_t size,
    void (*callback)(std::expected<std::span<uint8_t>, int>))
    -> std::expected<void, int> {
  callback(ReceiveData(address, size));
  return {};
}

auto HostI2CController::Receive(const std::string_view& message)
    -> std::expected<std::string, common::Error> {
  static_cast<void>(message);
  return std::unexpected(common::Error::kUnhandled);
}

}  // namespace mcu
