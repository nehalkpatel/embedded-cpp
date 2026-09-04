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
namespace board {

/// Satisfies both pin interfaces: OutputPin inherits InputPin virtually, so a
/// stand-in for an output pin must answer Get() and SetInterruptHandler() too.
class UnimplementedPin final : public mcu::BidirectionalPin {
 public:
  [[nodiscard]] auto Get()
      -> std::expected<mcu::PinState, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto SetInterruptHandler(std::function<void()> /*handler*/,
                                         mcu::PinTransition /*transition*/)
      -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto SetHigh() -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto SetLow() -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto Toggle() -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto Configure(mcu::PinDirection /*direction*/)
      -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }
};

class UnimplementedUart final : public mcu::Uart {
 public:
  [[nodiscard]] auto Init(const mcu::UartConfig& /*config*/)
      -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto Send(std::span<const std::byte> /*data*/)
      -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto Receive(std::span<std::byte> /*buffer*/,
                             std::uint32_t /*timeout_ms*/)
      -> std::expected<std::size_t, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  [[nodiscard]] auto SetRxHandler(
      std::function<void(const std::byte*, std::size_t)> /*handler*/)
      -> std::expected<void, common::Error> override {
    return std::unexpected(common::Error::kInvalidOperation);
  }
};

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
