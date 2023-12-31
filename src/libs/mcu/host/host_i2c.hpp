#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <unordered_map>

#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

class HostI2CController final : public mcu::I2CController {
 public:
  HostI2CController() = default;
  HostI2CController(const HostI2CController&) = delete;
  HostI2CController(HostI2CController&&) = delete;
  auto operator=(const HostI2CController&) -> HostI2CController& = delete;
  auto operator=(HostI2CController&&) -> HostI2CController& = delete;
  ~HostI2CController() override = default;

  auto SendData(uint16_t address, std::span<const uint8_t> data)
      -> std::expected<void, int> override;

  auto ReceiveData(uint16_t address, size_t size)
      -> std::expected<std::span<uint8_t>, int> override;

  virtual auto SendDataInterrupt(uint16_t address,
                                 std::span<const uint8_t> data,
                                 void (*callback)(std::expected<void, int>))
      -> std::expected<void, int> override;
  virtual auto ReceiveDataInterrupt(
      uint16_t address, size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, int>))
      -> std::expected<void, int> override;

  virtual auto SendDataDma(uint16_t address, std::span<const uint8_t> data,
                           void (*callback)(std::expected<void, int>))
      -> std::expected<void, int> override;
  virtual auto ReceiveDataDma(
      uint16_t address, size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, int>))
      -> std::expected<void, int> override;

 private:
  std::unordered_map<uint16_t, std::array<uint8_t, 256>> data_buffers_;
};
}  // namespace mcu
