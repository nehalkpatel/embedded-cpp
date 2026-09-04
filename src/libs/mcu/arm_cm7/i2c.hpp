#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/i2c.hpp"

namespace mcu {

/// @brief Which I2C instance. As with UsartId, the enumerators are indices,
/// not addresses: `I2C1` is a CMSIS macro, so no public header here may name
/// it. `kI2C1` is a distinct token and safe.
enum class I2CId : std::uint8_t { kI2C1, kI2C2, kI2C3, kI2C4 };

/// @brief Where an I2C instance's two pins are, and their alternate function.
struct I2CPins {
  GpioPort scl_port;
  std::uint32_t scl_pin;
  GpioPort sda_port;
  std::uint32_t sda_pin;
  std::uint8_t alternate_function;
};

/// @brief Blocking I2C bus master.
///
/// Transfers use the peripheral's autoend mode: NBYTES is programmed up front
/// and the hardware issues STOP on its own. That covers a whole transfer in
/// one setup, and rules out the repeated-start sequences a register-level
/// read of an addressed device needs -- which is why the demo application
/// writes and reads as two separate transactions.
class I2CBus final : public I2CController {
 public:
  I2CBus(I2CId id, const I2CPins& pins) : id_(id), pins_(pins) {}

  /// @brief Enable the peripheral and configure its pins. Called by the board;
  /// SendData and ReceiveData report kInvalidState until it has run.
  ///
  /// That the board must call this is a convention, not something the type
  /// enforces -- and unlike Uart, mcu::I2CController has no Init(), so nothing
  /// in board::Board's shape hints that the call is required. See issue #37.
  ///
  /// The bus runs at 100 kHz; making the speed a board-supplied parameter is
  /// issue #38.
  [[nodiscard]] auto Init() -> std::expected<void, common::Error>;

  [[nodiscard]] auto SendData(std::uint16_t address,
                              std::span<const std::byte> data)
      -> std::expected<void, common::Error> override;

  [[nodiscard]] auto ReceiveData(std::uint16_t address,
                                 std::span<std::byte> buffer)
      -> std::expected<std::size_t, common::Error> override;

 private:
  I2CId id_;
  I2CPins pins_;
  bool initialized_ = false;
};

}  // namespace mcu
