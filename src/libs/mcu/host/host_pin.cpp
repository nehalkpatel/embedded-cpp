#include "host_pin.hpp"

#include <expected>
#include <span>
#include <string>

#include "emulator_message_json_encoder.hpp"
#include "host_emulator_messages.hpp"
#include "libs/common/error.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {
using json = nlohmann::json;

auto HostPin::Configure(PinDirection direction)
    -> std::expected<void, common::Error> {
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
auto HostPin::Get() -> std::expected<PinState, common::Error> {
  return GetState();
}

auto HostPin::SendState(PinState state) -> std::expected<void, common::Error> {
  const PinEmulatorRequest req = {
      .name = name_,
      .operation = OperationType::kSet,
      .state = state,
  };
  std::string str;
  Encode(str, req);
  if (!transport_.Send(str)) {
    return std::unexpected(common::Error::kUnknown);
  }
  auto rx_bytes = transport_.Receive();
  if (!rx_bytes) {
    return std::unexpected(common::Error::kUnknown);
  }

  PinEmulatorResponse resp;
  Decode(rx_bytes.value(), resp);

  if (resp.status != common::Error::kOk) {
    return std::unexpected(resp.status);
  }

  return {};
}

auto HostPin::GetState() -> std::expected<PinState, common::Error> {
  const PinEmulatorRequest req = {
      .name = name_,
      .operation = OperationType::kGet,
  };
  std::string str;
  Encode(str, req);
  if (!transport_.Send(str)) {
    return std::unexpected(common::Error::kUnknown);
  }
  auto rx_bytes = transport_.Receive();
  if (!rx_bytes) {
    return std::unexpected(common::Error::kUnknown);
  }
  PinEmulatorResponse resp;
  Decode(rx_bytes.value(), resp);

  return resp.state;
}
}  // namespace mcu
