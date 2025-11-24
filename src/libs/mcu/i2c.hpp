#pragma once

#include <cstdint>
#include <expected>
#include <span>

#include "libs/common/error.hpp"

namespace mcu {

class I2CController {
 public:
  virtual ~I2CController() = default;

  virtual auto SendData(uint16_t address, std::span<const uint8_t> data)
      -> std::expected<void, common::Error> = 0;

  virtual auto ReceiveData(uint16_t address, size_t size)
      -> std::expected<std::span<uint8_t>, common::Error> = 0;

  virtual auto SendDataInterrupt(
      uint16_t address, std::span<const uint8_t> data,
      void (*callback)(std::expected<void, common::Error>))
      -> std::expected<void, common::Error> = 0;
  virtual auto ReceiveDataInterrupt(
      uint16_t address, size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, common::Error>))
      -> std::expected<void, common::Error> = 0;

  virtual auto SendDataDma(uint16_t address, std::span<const uint8_t> data,
                           void (*callback)(std::expected<void, common::Error>))
      -> std::expected<void, common::Error> = 0;
  virtual auto ReceiveDataDma(
      uint16_t address, size_t size,
      void (*callback)(std::expected<std::span<uint8_t>, common::Error>))
      -> std::expected<void, common::Error> = 0;
};

}  // namespace mcu
