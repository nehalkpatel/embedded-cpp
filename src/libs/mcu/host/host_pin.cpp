#include "host_pin.hpp"

#include <expected>
#include <string>

#include "nlohmann/json.hpp"
#include "zmq.hpp"

namespace common {

NLOHMANN_JSON_SERIALIZE_ENUM(Error,
                             {
                                 {Error::kOk, "Ok"},
                                 {Error::kUnknown, "Unknown"},
                                 {Error::kInvalidArgument, "InvalidArgument"},
                                 {Error::kInvalidState, "InvalidState"},
                                 {Error::kInvalidOperation, "InvalidOperation"},
                             })

} // namespace common

namespace mcu {
using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(PinState, {
                                           {PinState::kLow, "Low"},
                                           {PinState::kHigh, "High"},
                                           {PinState::kHighZ, "Hi_Z"},
                                       })

struct PinEmulatorRequest {
  std::string type = "request";
  std::string object = "pin";
  std::string name;
  std::string operation;
  PinState state;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PinEmulatorRequest, type, object, name,
                                   operation, state)

struct PinEmulatorResponse {
  std::string type = "response";
  std::string object = "pin";
  std::string name;
  PinState state;
  common::Error status;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PinEmulatorResponse, type, object, name,
                                   state, status)

auto HostPin::Configure(PinDirection direction) -> std::expected<void, common::Error> {
  direction_ = direction;
  return {};
}
auto HostPin::SetHigh() -> std::expected<void, common::Error> {
  if (direction_ == PinDirection::kInput) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  return SendState(PinState::kHigh);
}
auto HostPin::SetLow() -> std::expected<void, common::Error> {
  if (direction_ == PinDirection::kInput) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  return SendState(PinState::kLow);
}
auto HostPin::Get() -> std::expected<PinState, common::Error> { return GetState(); }

auto HostPin::SendState(PinState state) -> std::expected<void, common::Error> {
  PinEmulatorRequest req = {
      .name = name_,
      .operation = "set",
      .state = state,
  };
  json j_pin = req;
  if (sref_.send(zmq::buffer(j_pin.dump()), zmq::send_flags::dontwait) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }
  zmq::message_t msg;
  if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }
  json j = json::parse(msg.to_string());
  PinEmulatorResponse resp = j;
  if (resp.status != common::Error::kOk) {
    return std::unexpected(resp.status);
  }

  return {};
}
auto HostPin::GetState() -> std::expected<PinState, common::Error> {
  PinEmulatorRequest req = {
      .name = name_,
      .operation = "get",
  };
  json j_pin = req;
  if (sref_.send(zmq::buffer(j_pin.dump()), zmq::send_flags::dontwait) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }
  zmq::message_t msg;
  if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }
  json j = json::parse(msg.to_string());

  PinEmulatorResponse resp = j;

  return resp.state;
}
}  // namespace mcu
