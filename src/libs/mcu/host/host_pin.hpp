#pragma once

#include <string>

#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/host/transport.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

class HostPin final : public BidirectionalPin, public Receiver {
 public:
  explicit HostPin(std::string name, Transport& transport)
      : name_{std::move(name)}, transport_{transport} {}
  ~HostPin() override = default;
  HostPin(const HostPin&) = delete;
  HostPin(HostPin&&) = delete;
  auto operator=(const HostPin&) -> HostPin& = delete;
  auto operator=(HostPin&&) -> HostPin& = delete;

  auto Configure(PinDirection direction)
      -> std::expected<void, common::Error> override;
  auto SetHigh() -> std::expected<void, common::Error> override;
  auto SetLow() -> std::expected<void, common::Error> override;
  auto Get() -> std::expected<PinState, common::Error> override;

  auto Receive(const std::string_view& message)
      -> std::expected<std::string, common::Error> override;

 private:
  auto SendState(PinState state) -> std::expected<void, common::Error>;
  auto GetState() -> std::expected<PinState, common::Error>;

  const std::string name_;
  Transport& transport_;
  PinDirection direction_;
  PinState state_;
};

}  // namespace mcu
