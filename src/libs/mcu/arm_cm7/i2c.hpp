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

/// @brief How fast the bus runs.
///
/// A property of the bus, not of a link: every device on a shared bus has to
/// agree, and the achievable rate depends on pull-up strength, trace length and
/// the slowest device present. All of that is board knowledge, which is why the
/// board names it and mcu::I2CController does not carry it.
enum class I2CSpeed : std::uint8_t { kStandard100kHz = 1, kFast400kHz };

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
  /// Constructing the bus brings it up: this enables the peripheral clock,
  /// configures the pins and programs TIMINGR, so an I2CBus that exists is one
  /// that works. The board supplies the speed because the wiring, not the
  /// application, is what determines the rate the bus can carry.
  I2CBus(I2CId bus_id, const I2CPins& pins, I2CSpeed speed);

  [[nodiscard]] auto SendData(std::uint16_t address,
                              std::span<const std::byte> data)
      -> std::expected<void, common::Error> override;

  [[nodiscard]] auto ReceiveData(std::uint16_t address,
                                 std::span<std::byte> buffer)
      -> std::expected<std::size_t, common::Error> override;

 private:
  I2CId id_;
  I2CPins pins_;
};

}  // namespace mcu
