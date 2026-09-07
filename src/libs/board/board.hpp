#pragma once

#include <cstdint>
#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

namespace board {

/// @brief Everything an application is given of the hardware it runs on.
///
/// A board comes up in two phases, and which one a peripheral belongs to is
/// settled by whether it can fail, not by what is convenient:
///
///   - **Construction** brings up what is inside the MCU. Register writes with
///     no dependencies, so they cannot fail and need no timebase.
///   - **Init()** brings up what is on the other side of a wire, and is
///     fallible for that reason: a device can be absent, wrongly strapped, or
///     simply not answer. It also starts the system tick, which is what makes
///     bounded waits -- and so any real bus transfer -- legal from here on.
///
/// docs/BOOT_FLOW.md has the whole sequence and the rules for each stage.
struct Board {
  virtual ~Board() = default;

  /// Bring up everything off-chip and start the timebase the drivers need.
  /// Must have returned successfully before any accessor below is used.
  [[nodiscard]] virtual auto Init() -> std::expected<void, common::Error> = 0;
  [[nodiscard]] virtual auto UserLed1() -> mcu::OutputPin& = 0;
  [[nodiscard]] virtual auto UserLed2() -> mcu::OutputPin& = 0;
  [[nodiscard]] virtual auto UserButton1() -> mcu::InputPin& = 0;
  [[nodiscard]] virtual auto I2C1() -> mcu::I2CController& = 0;
  [[nodiscard]] virtual auto Uart1() -> mcu::Uart& = 0;
};
}  // namespace board
