#include "host_pin.hpp"

#include <expected>

#include "libs/common/error.hpp"
#include "libs/mcu/host/emulator_message_json_encoder.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
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

// Messages received from the external application will always be
// requests. HostPin will only send responses.
auto HostPin::Receive(const std::string_view& message)
    -> std::expected<std::string, common::Error> {
  const auto json_pin = json::parse(message);
  if (json_pin["name"] != name_) {
    return {};
  }
  if (json_pin["type"] == MessageType::kRequest) {
    const auto req = Decode<PinEmulatorRequest>(message);
    if (req.operation == OperationType::kGet) {
      const PinEmulatorResponse resp = {
          .type = MessageType::kResponse,
          .object = ObjectType::kPin,
          .name = name_,
          .state = direction_ == PinDirection::kInput ? GetState().value()
                                                      : PinState::kHigh,
          .status = common::Error::kOk,
      };
      return Encode(resp);
    }
    if (req.operation == OperationType::kSet) {
      if (direction_ == PinDirection::kInput) {
        const PinEmulatorResponse resp = {
            .status = common::Error::kInvalidOperation,
        };
        return Encode(resp);
      }
      if (req.state == PinState::kHigh) {
        SetHigh();
      } else {
        SetLow();
      }
    }
  }
  return {};
}

}  // namespace mcu
