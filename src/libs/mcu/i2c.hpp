#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <unordered_map>

namespace mcu {

struct I2CController {
  virtual ~I2CController() = default;

  virtual auto SendData(uint16_t address, std::span<const uint8_t> data)
      -> std::expected<void, int> = 0;

  virtual auto ReceiveData(uint16_t address, size_t size)
      -> std::expected<std::span<uint8_t>, int> = 0;

  virtual auto SendDataInterrupt(uint16_t address,
                                 std::span<const uint8_t> data,
                                 void (*callback)(std::expected<void, int>))
      -> std::expected<void, int> = 0;
  virtual auto ReceiveDataInterrupt(
      uint16_t address,
      size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, int>))
      -> std::expected<void, int> = 0;

  virtual auto SendDataDma(uint16_t address, std::span<const uint8_t> data,
                           void (*callback)(std::expected<void, int>))
      -> std::expected<void, int> = 0;
  virtual auto ReceiveDataDma(
      uint16_t address,
      size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, int>))
      -> std::expected<void, int> = 0;
};

}  // namespace mcu
