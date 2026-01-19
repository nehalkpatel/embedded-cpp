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

  [[nodiscard]] virtual auto SendData(uint16_t address,
                                      std::span<const std::byte> data)
      -> std::expected<void, common::Error> = 0;

  /// @brief Receive data from I2C device into caller-provided buffer
  /// @param address I2C device address
  /// @param buffer Caller-provided buffer to store received data
  /// @return Number of bytes actually received, or error
  [[nodiscard]] virtual auto ReceiveData(uint16_t address,
                                         std::span<std::byte> buffer)
      -> std::expected<size_t, common::Error> = 0;

  [[nodiscard]] virtual auto SendDataInterrupt(
      uint16_t address, std::span<const std::byte> data,
      std::function<void(std::expected<void, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto ReceiveDataInterrupt(
      uint16_t address, std::span<std::byte> buffer,
      std::function<void(std::expected<size_t, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;

  [[nodiscard]] virtual auto SendDataDma(
      uint16_t address, std::span<const std::byte> data,
      std::function<void(std::expected<void, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto ReceiveDataDma(
      uint16_t address, std::span<std::byte> buffer,
      std::function<void(std::expected<size_t, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;
};

}  // namespace mcu
