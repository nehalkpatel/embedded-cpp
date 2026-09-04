#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>

#include "libs/common/error.hpp"
#include "libs/mcu/i2c.hpp"
#include "libs/mcu/pin.hpp"
#include "libs/mcu/uart.hpp"

/// @file
/// Placeholders for the peripherals this board has not implemented yet.
///
/// board::Board is an all-or-nothing interface: a board that implements none
/// of it does not compile, and one that implements only pins cannot be
/// constructed. These stubs let the board satisfy the interface from the
/// first commit, so every application links and the hardware bring-up can
/// proceed one peripheral at a time instead of all at once.
///
/// Each returns kInvalidOperation rather than pretending to succeed: an
/// application that reaches for an unimplemented peripheral gets an error at
/// the call, not silence. They are deleted as the real implementations land.
///
/// These have a delete-by date: B8 removes UnimplementedI2CController, the
/// last one. (UnimplementedPin went with B4, UnimplementedUart with B7.) A stub
/// still here after B8 has stopped being bring-up scaffolding and become
/// evidence of a separate problem -- that board::Board cannot express "this
/// board does not have that peripheral" (see Milestone 3 in
/// docs/PROJECT_PLAN.md).
namespace board {

class UnimplementedI2CController final : public mcu::I2CController {
 public:
  [[nodiscard]] auto SendData(std::uint16_t /*address*/,
                              std::span<const std::byte> /*data*/)
      -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto ReceiveData(std::uint16_t /*address*/,
                                 std::span<std::byte> /*buffer*/)
      -> std::expected<std::size_t, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }
};

}  // namespace board
