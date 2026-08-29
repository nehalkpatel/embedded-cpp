#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>

#include "libs/common/error.hpp"

namespace mcu {

/// @brief UART configuration parameters
struct UartConfig {
  uint32_t baud_rate{115200};

  enum class DataBits : uint8_t {
    k7Bits = 7,
    k8Bits = 8,
    k9Bits = 9
  } data_bits{DataBits::k8Bits};

  enum class Parity : uint8_t {
    kNone,
    kEven,
    kOdd,
  } parity{Parity::kNone};

  enum class StopBits : uint8_t {
    k1Bit,
    k2Bits,
  } stop_bits{StopBits::k1Bit};

  enum class FlowControl : uint8_t {
    kNone,
    kRtsCts,
    kXonXoff
  } flow_control{FlowControl::kNone};
};

/// @brief UART peripheral interface: blocking transfers plus an RxHandler for
/// unsolicited incoming data.
/// Async (interrupt/DMA-driven) send and receive are future work: they join
/// this interface when a hardware platform can implement them with genuinely
/// different behavior (see docs/PROJECT_PLAN.md).
class Uart {
 public:
  virtual ~Uart() = default;

  /// @brief Initialize UART with configuration
  /// @param config UART configuration parameters
  /// @return Success or error code
  [[nodiscard]] virtual auto Init(const UartConfig& config)
      -> std::expected<void, common::Error> = 0;

  /// @brief Send data (blocking)
  /// @param data Span of bytes to send
  /// @return Success or error code
  [[nodiscard]] virtual auto Send(std::span<const std::byte> data)
      -> std::expected<void, common::Error> = 0;

  /// @brief Receive data (blocking with timeout)
  /// @param buffer Buffer to store received data
  /// @param timeout_ms Timeout in milliseconds (0 = wait forever)
  /// @return Number of bytes received or error
  [[nodiscard]] virtual auto Receive(std::span<std::byte> buffer,
                                     uint32_t timeout_ms)
      -> std::expected<size_t, common::Error> = 0;

  /// @brief Set handler for unsolicited incoming data
  /// Similar to pin interrupts, this allows the UART to notify the
  /// application when data arrives asynchronously (e.g., from external source)
  /// @param handler Callback invoked when data arrives (data pointer and size)
  /// @return Success or error code
  [[nodiscard]] virtual auto SetRxHandler(
      std::function<void(const std::byte*, size_t)> handler)
      -> std::expected<void, common::Error> = 0;
};

}  // namespace mcu
