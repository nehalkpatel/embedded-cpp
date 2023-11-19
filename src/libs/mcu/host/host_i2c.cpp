#include "host_i2c.hpp"

namespace mcu {
auto HostI2CController::SendData(uint16_t address,
                                 std::span<const uint8_t> data)
    -> std::expected<void, int> {
  std::copy(data.begin(), data.end(), data_buffers_[address].begin());
  return {};
}

auto HostI2CController::ReceiveData(uint16_t address, size_t size)
    -> std::expected<std::span<uint8_t>, int> {
  return std::span<uint8_t>(data_buffers_[address].data(),
                            std::min(size, data_buffers_[address].size()));
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

auto HostI2CController::SendDataDma(uint16_t address,
                                    std::span<const uint8_t> data,
                                    void (*callback)(std::expected<void, int>))
    -> std::expected<void, int> {
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

}  // namespace mcu
