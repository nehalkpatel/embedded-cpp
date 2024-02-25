#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <unordered_map>

#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/i2c.hpp"

namespace mcu {

class HostI2CController final : public I2CController, public Receiver {
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

  auto SendDataInterrupt(uint16_t address, std::span<const uint8_t> data,
                         void (*callback)(std::expected<void, int>))
      -> std::expected<void, int> override;
  auto ReceiveDataInterrupt(
      uint16_t address, size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, int>))
      -> std::expected<void, int> override;

  auto SendDataDma(uint16_t address, std::span<const uint8_t> data,
                   void (*callback)(std::expected<void, int>))
      -> std::expected<void, int> override;
  auto ReceiveDataDma(uint16_t address, size_t size,
                      void (*callback)(std::expected<std::span<uint8_t>, int>))
      -> std::expected<void, int> override;
  auto Receive(const std::string_view& message)
      -> std::expected<std::string, common::Error> override;

 private:
  std::unordered_map<uint16_t, std::array<uint8_t, 256>> data_buffers_;
};
}  // namespace mcu
