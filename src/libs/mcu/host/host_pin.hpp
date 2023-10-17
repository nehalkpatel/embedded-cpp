#pragma once

#include <string>

#include "pin.hpp"
#include "zmq.hpp"

namespace mcu {

struct HostPin : public mcu::Pin {
  explicit HostPin(const std::string name, zmq::socket_ref sref)
      : name_(name), sref_(sref) {}
  ~HostPin() override = default;
  HostPin(const HostPin&) = delete;
  HostPin(HostPin&&) = delete;
  auto operator=(const HostPin&) -> HostPin& = delete;
  auto operator=(HostPin&&) -> HostPin& = delete;

  auto Configure(PinDirection direction) -> std::expected<void, Error> override;
  auto SetHigh() -> std::expected<void, Error> override;
  auto SetLow() -> std::expected<void, Error> override;
  auto Get() -> std::expected<PinState, Error> override;

 private:
  auto SendState(PinState state) -> std::expected<void, Error>;
  auto GetState() -> std::expected<PinState, Error>;

  const std::string name_;
  zmq::socket_ref sref_;
  PinDirection direction_;
};

}  // namespace mcu
