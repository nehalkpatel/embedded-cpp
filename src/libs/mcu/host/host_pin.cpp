#include "host_pin.hpp"

#include <expected>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"
#include "zmq.hpp"

namespace mcu {
using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(PinState, {
                                           {PinState::kHigh, "High"},
                                           {PinState::kLow, "Low"},
                                           {PinState::kHighZ, "Hi_Z"},
                                       })


  auto HostPin::Configure(PinDirection direction)
      -> std::expected<void, Error> {
    direction_ = direction;
    return {};
  }
  auto HostPin::SetHigh() -> std::expected<void, Error> {
    if (direction_ == PinDirection::kInput) {
      return std::unexpected(Error::kInvalidOperation);
    }
    return SendState(PinState::kHigh);
  }
  auto HostPin::SetLow() -> std::expected<void, Error> {
    if (direction_ == PinDirection::kInput) {
      return std::unexpected(Error::kInvalidOperation);
    }
    return SendState(PinState::kLow);
  }
  auto HostPin::Get() -> std::expected<PinState, Error> { return GetState(); }
  
  auto HostPin::SendState(PinState state) -> std::expected<void, Error> {
    json j_pin = {{"type", "request"}, {"object", "pin"}, {"operation", "set"}, {"pin", name_}, {"state", state}};
    if (sref_.send(zmq::buffer(j_pin.dump()), zmq::send_flags::dontwait) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    zmq::message_t msg;
    if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    json j = json::parse(msg.to_string());
    std::cout << "j: " << j << std::endl;

    if (j["status"] != "Ok") {
      return std::unexpected(Error::kUnknown);
    }
    return {};
  }
  auto HostPin::GetState() -> std::expected<PinState, Error> {
    json j_pin = {{"type", "request"}, {"object", "pin"}, {"operation", "get"}, {"pin", name_}};
    if (sref_.send(zmq::buffer(j_pin.dump()), zmq::send_flags::dontwait) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    zmq::message_t msg;
    if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
      return std::unexpected(Error::kUnknown);
    }
    json j = json::parse(msg.to_string());
    std::cout << "j: " << j << std::endl;
    return j["state"].get<PinState>();
  }
}  // namespace mcu