#include "host_pin.hpp"

#include <expected>

#include "host_emulator_messages.hpp" 
#include "emulator_message_json_encoder.hpp" 
#include "zmq.hpp"

namespace mcu {
using json = nlohmann::json;

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
      .operation = OperationType::kSet,
      .state = state,
  };
  std::string str;
  Encode(str, req);
  if (sref_.send(zmq::buffer(str), zmq::send_flags::dontwait) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }
  zmq::message_t msg;
  if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }

  PinEmulatorResponse resp;
  Decode(msg.to_string(), resp);

  if (resp.status != common::Error::kOk) {
    return std::unexpected(resp.status);
  }

  return {};
}

auto HostPin::GetState() -> std::expected<PinState, common::Error> {
  PinEmulatorRequest req = {
      .name = name_,
      .operation = OperationType::kGet,
  };
  std::string str;
  Encode(str, req);
  if (sref_.send(zmq::buffer(str), zmq::send_flags::dontwait) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }
  zmq::message_t msg;
  if (sref_.recv(msg, zmq::recv_flags::none) < 0) {
    return std::unexpected(common::Error::kUnknown);
  }

  PinEmulatorResponse resp;
  Decode(msg.to_string(), resp);

  return resp.state;
}
}  // namespace mcu
