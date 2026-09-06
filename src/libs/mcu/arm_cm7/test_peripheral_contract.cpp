/// @file
/// The arm_cm7 peripherals' bring-up contract, checked at compile time.
///
/// This is the only coverage the hardware backend has, and it is possible only
/// because its public headers name no vendor type: they include CMSIS nowhere,
/// so they compile on the host even though the backend itself does not. Nothing
/// here odr-uses a driver, so the binary links without arm_cm7 or CMSIS.
///
/// What it can prove is that the types make an unconfigured peripheral
/// unrepresentable. What it cannot prove is that the constructors write the
/// right bits -- that still needs the board in hand.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "libs/mcu/arm_cm7/gpio_pin.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/arm_cm7/i2c.hpp"
#include "libs/mcu/pin.hpp"

namespace {

// A pin cannot be made without saying which direction it is, and making one is
// what configures it. So an unconfigured GpioPin cannot be named. That is the
// whole of issue #37, stated as something the compiler checks.
static_assert(!std::is_default_constructible_v<mcu::GpioPin>);
static_assert(
    !std::is_constructible_v<mcu::GpioPin, mcu::GpioPort, std::uint32_t>);
static_assert(std::is_constructible_v<mcu::GpioPin, mcu::GpioPort,
                                      std::uint32_t, mcu::PinDirection>);

// It stays a BidirectionalPin: the direction is still switchable at run time,
// which is why the direction guards in gpio_pin.cpp are correct rather than
// leftovers.
static_assert(std::is_base_of_v<mcu::BidirectionalPin, mcu::GpioPin>);
static_assert(std::is_base_of_v<mcu::InputPin, mcu::GpioPin>);
static_assert(std::is_base_of_v<mcu::OutputPin, mcu::GpioPin>);

// Same contract for the bus, plus: no I2CBus without a speed. The board has to
// say how fast its wiring can go, because a default would let that stay
// invisible.
static_assert(!std::is_default_constructible_v<mcu::I2CBus>);
static_assert(!std::is_constructible_v<mcu::I2CBus, mcu::I2CId, mcu::I2CPins>);
static_assert(std::is_constructible_v<mcu::I2CBus, mcu::I2CId, mcu::I2CPins,
                                      mcu::I2CSpeed>);

// The peripherals are still usable through the portable interfaces the board
// hands out, which is what keeps applications unaware of any of this.
static_assert(std::is_base_of_v<mcu::I2CController, mcu::I2CBus>);

TEST(PeripheralContract, UnconfiguredPeripheralsAreUnrepresentable) {
  // The static_asserts above are the real test -- this binary failing to
  // compile is the failure mode. Restating one at run time gives CTest
  // something to report, so a silently dropped test target is visible.
  EXPECT_FALSE(
      (std::is_constructible_v<mcu::GpioPin, mcu::GpioPort, std::uint32_t>));
  EXPECT_FALSE(
      (std::is_constructible_v<mcu::I2CBus, mcu::I2CId, mcu::I2CPins>));
}

}  // namespace
