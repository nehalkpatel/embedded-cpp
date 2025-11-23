#pragma once

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

/// @brief UART peripheral interface
/// Implementations may use interrupts, DMA, or blocking internally
class Uart {
 public:
  virtual ~Uart() = default;

  /// @brief Initialize UART with configuration
  /// @param config UART configuration parameters
  /// @return Success or error code
  virtual auto Init(const UartConfig& config)
      -> std::expected<void, common::Error> = 0;

  /// @brief Send data (blocking)
  /// @param data Span of bytes to send
  /// @return Success or error code
  virtual auto Send(std::span<const uint8_t> data)
      -> std::expected<void, common::Error> = 0;

  /// @brief Receive data (blocking with timeout)
  /// @param buffer Buffer to store received data
  /// @param timeout_ms Timeout in milliseconds (0 = wait forever)
  /// @return Number of bytes received or error
  virtual auto Receive(std::span<uint8_t> buffer, uint32_t timeout_ms = 0)
      -> std::expected<size_t, common::Error> = 0;

  /// @brief Send data asynchronously
  /// Implementation may use interrupts or DMA
  /// @param data Span of bytes to send
  /// @param callback Called when transfer completes
  /// @return Success or error code
  virtual auto SendAsync(
      std::span<const uint8_t> data,
      std::function<void(std::expected<void, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;

  /// @brief Receive data asynchronously
  /// Implementation may use interrupts or DMA
  /// @param buffer Buffer to store received data
  /// @param callback Called when data is received (with number of bytes)
  /// @return Success or error code
  virtual auto ReceiveAsync(
      std::span<uint8_t> buffer,
      std::function<void(std::expected<size_t, common::Error>)> callback)
      -> std::expected<void, common::Error> = 0;

  /// @brief Check if UART is busy transmitting
  /// @return True if busy, false otherwise
  virtual auto IsBusy() -> bool = 0;

  /// @brief Get number of bytes available to read
  /// @return Number of bytes in receive buffer
  virtual auto Available() -> size_t = 0;

  /// @brief Flush transmit buffer (wait for all data to be sent)
  /// @return Success or error code
  virtual auto Flush() -> std::expected<void, common::Error> = 0;

  /// @brief Set handler for unsolicited incoming data
  /// Similar to pin interrupts, this allows the UART to notify the
  /// application when data arrives asynchronously (e.g., from external source)
  /// @param handler Callback invoked when data arrives (data pointer and size)
  /// @return Success or error code
  virtual auto SetRxHandler(std::function<void(const uint8_t*, size_t)> handler)
      -> std::expected<void, common::Error> = 0;
};

}  // namespace mcu
