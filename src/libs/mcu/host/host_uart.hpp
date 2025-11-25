#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/host/transport.hpp"
#include "libs/mcu/uart.hpp"

namespace mcu {

class HostUart final : public Uart, public Receiver {
 public:
  explicit HostUart(std::string name, Transport& transport)
      : name_{std::move(name)}, transport_{transport} {}
  ~HostUart() override = default;
  HostUart(const HostUart&) = delete;
  HostUart(HostUart&&) = delete;
  auto operator=(const HostUart&) -> HostUart& = delete;
  auto operator=(HostUart&&) -> HostUart& = delete;

  // Uart interface
  auto Init(const UartConfig& config)
      -> std::expected<void, common::Error> override;

  auto Send(std::span<const std::byte> data)
      -> std::expected<void, common::Error> override;

  auto Receive(std::span<std::byte> buffer, uint32_t timeout_ms)
      -> std::expected<size_t, common::Error> override;

  auto SendAsync(std::span<const std::byte> data,
                 std::function<void(std::expected<void, common::Error>)>
                     callback) -> std::expected<void, common::Error> override;

  auto ReceiveAsync(
      std::span<std::byte> buffer,
      std::function<void(std::expected<size_t, common::Error>)> callback)
      -> std::expected<void, common::Error> override;

  auto IsBusy() const -> bool override;
  auto Available() const -> size_t override;
  auto Flush() -> std::expected<void, common::Error> override;

  auto SetRxHandler(std::function<void(const std::byte*, size_t)> handler)
      -> std::expected<void, common::Error> override;

  // Receiver interface for handling async responses from emulator
  auto Receive(const std::string_view& message)
      -> std::expected<std::string, common::Error> override;

 private:
  const std::string name_;
  Transport& transport_;
  UartConfig config_{};
  bool initialized_{false};
  bool busy_{false};

  // Async callback storage
  std::function<void(std::expected<void, common::Error>)> send_callback_{};
  std::function<void(std::expected<size_t, common::Error>)> receive_callback_{};

  // Receive handler for unsolicited incoming data
  std::function<void(const std::byte*, size_t)> rx_handler_{};

  // Receive buffer for async operations
  std::vector<std::byte> receive_buffer_{};
};

}  // namespace mcu
