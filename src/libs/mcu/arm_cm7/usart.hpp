#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/uart.hpp"

namespace mcu {

/// @brief Which USART/UART instance. Values are indices into the driver's
/// internal tables, not addresses.
///
/// Named `kUsart3` rather than `USART3` deliberately: CMSIS defines `USART3`
/// as an object-like macro, so a public header that used the bare name would
/// rewrite it wherever it appeared downstream. Nothing in this header names a
/// vendor type; see the note in gpio_port.hpp.
enum class UsartId : std::uint8_t { kUsart1, kUsart2, kUsart3, kUsart6 };

/// @brief Where a USART's two pins are, and which alternate function selects
/// the peripheral on them.
struct UsartPins {
  GpioPort tx_port;
  std::uint32_t tx_pin;
  GpioPort rx_port;
  std::uint32_t rx_pin;
  std::uint8_t alternate_function;
};

/// @brief Blocking USART, with an optional receive interrupt.
///
/// Like every peripheral here, this is constructed inert and made real by a
/// separate call -- Init(), which the application makes. Nothing enforces
/// that; see issue #37.
///
/// Send and Receive poll the status register; there is no transmit buffering,
/// so Send returns once the last byte has left the shift register and the line
/// is idle. That is the honest shape for this peripheral until there is a
/// reason for more -- see the note on async modes in libs/mcu/uart.hpp.
class Usart final : public Uart {
 public:
  Usart(UsartId id, const UsartPins& pins) : id_(id), pins_(pins) {}

  [[nodiscard]] auto Init(const UartConfig& config)
      -> std::expected<void, common::Error> override;

  [[nodiscard]] auto Send(std::span<const std::byte> data)
      -> std::expected<void, common::Error> override;

  [[nodiscard]] auto Receive(std::span<std::byte> buffer,
                             std::uint32_t timeout_ms)
      -> std::expected<std::size_t, common::Error> override;

  [[nodiscard]] auto SetRxHandler(
      std::function<void(const std::byte*, std::size_t)> handler)
      -> std::expected<void, common::Error> override;

 private:
  UsartId id_;
  UsartPins pins_;
  bool initialized_ = false;
};

/// @brief Write one byte to whichever USART was last initialized.
///
/// Exists for newlib's _write, which has no way to reach a C++ object. Returns
/// false if no USART is up yet, so early output is dropped rather than
/// hanging on a peripheral that will never assert TXE.
auto PutcharToConsole(char value) -> bool;

}  // namespace mcu
