#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "libs/common/error.hpp"

namespace mcu {

/// @brief I2C controller (bus master) interface, blocking transfers only.
/// Interrupt- and DMA-driven transfer modes are future work: they join this
/// interface when a hardware platform can implement them with genuinely
/// different behavior (see docs/PROJECT_PLAN.md).
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
};

}  // namespace mcu
