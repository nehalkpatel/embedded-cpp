#include "host_pin.hpp"

#include <expected>
#include <functional>

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

auto HostPin::SetInterruptHandler(std::function<void()> handler,
                                  PinTransition transition)
    -> std::expected<void, common::Error> {
  handler_ = handler;
  transition_ = transition;
  return {};
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

  state_ = state;
  return {};
}

auto HostPin::CheckAndInvokeHandler(PinState prev_state, PinState cur_state)
    -> void {
  const bool interrupt_occurred{cur_state != prev_state};
  if (direction_ == PinDirection::kInput && interrupt_occurred) {
    if ((transition_ == PinTransition::kRising &&
         cur_state == PinState::kHigh) ||
        ((transition_ == PinTransition::kFalling &&
          cur_state == PinState::kLow)) ||
        (transition_ == PinTransition::kBoth)) {
      handler_();
    }
  }
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

  // If the MCU is polling the input, then it should NOT be configured
  // for interrupts. Therefore, we should not invoke the handler.
  // const PinState prev_state{state_};
  state_ = resp.state;
  // CheckAndInvokeHandler(prev_state, resp.state);

  return resp.state;
}

// Messages received from the external application will always be
// requests. HostPin will only send responses.
auto HostPin::Receive(const std::string_view& message)
    -> std::expected<std::string, common::Error> {
  const auto json_pin = json::parse(message);
  if (json_pin["name"] != name_) {
    return std::unexpected(common::Error::kInvalidArgument);
  }
  if (json_pin["type"] == MessageType::kResponse) {
    return std::unexpected(common::Error::kInvalidOperation);
  }
  PinEmulatorResponse resp = {
      .type = MessageType::kResponse,
      .object = ObjectType::kPin,
      .name = name_,
      .state = state_,
      .status = common::Error::kInvalidOperation,
  };
  const auto req = Decode<PinEmulatorRequest>(message);
  if (req.operation == OperationType::kGet) {
    resp.status = common::Error::kOk;
    return Encode(resp);
  }
  // Set from the external world is only allowed if the pin is an input
  // with respect to the MCU
  if (req.operation == OperationType::kSet) {
    if (direction_ == PinDirection::kOutput) {
      resp.status = common::Error::kInvalidOperation;
      return Encode(resp);
    }
    // The external entity pushed a pin update to the MCU.
    // Therefore check for interrupt.
    const PinState prev_state{state_};
    state_ = req.state;
    CheckAndInvokeHandler(prev_state, req.state);
    resp.state = state_;
    resp.status = common::Error::kOk;
    return Encode(resp);
  }
  return std::unexpected(common::Error::kInvalidOperation);
}

}  // namespace mcu
