#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>

#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/host/transport.hpp"
#include "libs/mcu/i2c.hpp"

namespace mcu {

class HostI2CController final : public I2CController, public Receiver {
 public:
  explicit HostI2CController(std::string name, Transport& transport)
      : name_{std::move(name)}, transport_{transport} {}
  HostI2CController(const HostI2CController&) = delete;
  HostI2CController(HostI2CController&&) = delete;
  auto operator=(const HostI2CController&) -> HostI2CController& = delete;
  auto operator=(HostI2CController&&) -> HostI2CController& = delete;
  ~HostI2CController() override = default;

  auto SendData(uint16_t address, std::span<const std::byte> data)
      -> std::expected<void, common::Error> override;

  auto ReceiveData(uint16_t address, std::span<std::byte> buffer)
      -> std::expected<size_t, common::Error> override;

  auto SendDataInterrupt(
      uint16_t address, std::span<const std::byte> data,
      std::function<void(std::expected<void, common::Error>)> callback)
      -> std::expected<void, common::Error> override;
  auto ReceiveDataInterrupt(
      uint16_t address, std::span<std::byte> buffer,
      std::function<void(std::expected<size_t, common::Error>)> callback)
      -> std::expected<void, common::Error> override;

  auto SendDataDma(uint16_t address, std::span<const std::byte> data,
                   std::function<void(std::expected<void, common::Error>)>
                       callback) -> std::expected<void, common::Error> override;
  auto ReceiveDataDma(
      uint16_t address, std::span<std::byte> buffer,
      std::function<void(std::expected<size_t, common::Error>)> callback)
      -> std::expected<void, common::Error> override;
  auto Receive(const std::string_view& message)
      -> std::expected<std::string, common::Error> override;

 private:
  const std::string name_;
  Transport& transport_;
};
}  // namespace mcu
