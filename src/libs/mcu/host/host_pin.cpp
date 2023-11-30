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
  if (!transport_.Send(Encode(req))) {
    return std::unexpected(common::Error::kUnknown);
  }
  auto rx_bytes = transport_.Receive();
  if (!rx_bytes) {
    return std::unexpected(common::Error::kUnknown);
  }

  const auto resp = Decode<PinEmulatorResponse>(rx_bytes.value());

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
  if (!transport_.Send(Encode(req))) {
    return std::unexpected(common::Error::kUnknown);
  }
  auto rx_bytes = transport_.Receive();
  if (!rx_bytes) {
    return std::unexpected(common::Error::kUnknown);
  }
  const auto resp = Decode<PinEmulatorResponse>(rx_bytes.value());

  return resp.state;
}
}  // namespace mcu
