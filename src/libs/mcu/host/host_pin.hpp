#pragma once

#include <string>

#include "nlohmann/json.hpp"
#include "pin.hpp"
#include "zmq.hpp"

namespace mcu {
using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(PinState, {
                                           {PinState::kHigh, "High"},
                                           {PinState::kLow, "Low"},
                                           {PinState::kHighZ, "Hi-Z"},
                                       })

struct HostPin : public mcu::Pin {
  explicit HostPin(const std::string name, zmq::socket_ref sref)
      : name_(name), sref_(sref) {}
  ~HostPin() override = default;
  HostPin(const HostPin&) = delete;
  HostPin(HostPin&&) = delete;
  auto operator=(const HostPin&) -> HostPin& = delete;
  auto operator=(HostPin&&) -> HostPin& = delete;

  auto Configure(PinDirection direction)
      -> std::expected<void, Error> override {
    direction_ = direction;
    return {};
  }
  auto SetHigh() -> std::expected<void, Error> override {
    if (direction_ == PinDirection::kInput) {
      return std::unexpected(Error::kInvalidOperation);
    }
    return SendState(PinState::kHigh);
  }
  auto SetLow() -> std::expected<void, Error> override {
    if (direction_ == PinDirection::kInput) {
      return std::unexpected(Error::kInvalidOperation);
    }
    return SendState(PinState::kLow);
  }
  auto Get() -> std::expected<PinState, Error> override { return GetState(); }

 private:
  auto SendState(PinState state) -> std::expected<void, Error> {
    json j_pin = {{"type", "request"}, {"object", "pin"}, {"operation", "set"}, {"pin", name_}, {"state", state}};
    if (sref_.send(zmq::buffer(j_pin.dump()), zmq::send_flags::dontwait) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    return {};
  }
  auto GetState() -> std::expected<PinState, Error> {
    json j_pin = {{"type", "request"}, {"object", "pin"}, {"operation", "get"}, {"pin", name_}};
    if (sref_.send(zmq::buffer(j_pin.dump()), zmq::send_flags::dontwait) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    zmq::message_t msg;
    if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    json j = json::parse(msg.to_string());
    return j["state"].get<PinState>();
  }
  const std::string name_;
  zmq::socket_ref sref_;
  PinDirection direction_;
};

}  // namespace mcu
