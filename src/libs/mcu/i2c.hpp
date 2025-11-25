#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>

#include "libs/common/error.hpp"

namespace mcu {

class I2CController {
 public:
  virtual ~I2CController() = default;

  virtual auto SendData(uint16_t address, std::span<const std::byte> data)
      -> std::expected<void, common::Error> = 0;

  virtual auto ReceiveData(uint16_t address, size_t size)
      -> std::expected<std::span<std::byte>, common::Error> = 0;

  virtual auto SendDataInterrupt(
      uint16_t address, std::span<const std::byte> data,
      std::function<void(std::expected<void, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;
  virtual auto ReceiveDataInterrupt(
      uint16_t address, size_t size,
      std::function<void(std::expected<std::span<std::byte>, common::Error>)>
          callback) -> std::expected<void, common::Error> = 0;

  virtual auto SendDataDma(
      uint16_t address, std::span<const std::byte> data,
      std::function<void(std::expected<void, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;
  virtual auto ReceiveDataDma(
      uint16_t address, size_t size,
      std::function<void(std::expected<std::span<std::byte>, common::Error>)>
          callback) -> std::expected<void, common::Error> = 0;
};

}  // namespace mcu
