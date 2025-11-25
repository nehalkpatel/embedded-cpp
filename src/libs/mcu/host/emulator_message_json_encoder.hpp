#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "libs/common/error.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/pin.hpp"

namespace common {

NLOHMANN_JSON_SERIALIZE_ENUM(Error,
                             {
                                 {Error::kOk, "Ok"},
                                 {Error::kUnknown, "Unknown"},
                                 {Error::kInvalidArgument, "InvalidArgument"},
                                 {Error::kInvalidState, "InvalidState"},
                                 {Error::kInvalidOperation, "InvalidOperation"},
                             })

}  // namespace common

namespace mcu {

using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(PinState, {
                                           {PinState::kLow, "Low"},
                                           {PinState::kHigh, "High"},
                                           {PinState::kHighZ, "Hi_Z"},
                                       })

NLOHMANN_JSON_SERIALIZE_ENUM(PinDirection,
                             {
                                 {PinDirection::kInput, "Input"},
                                 {PinDirection::kOutput, "Output"},
                             })

NLOHMANN_JSON_SERIALIZE_ENUM(MessageType,
                             {
                                 {MessageType::kRequest, "Request"},
                                 {MessageType::kResponse, "Response"},
                             })

NLOHMANN_JSON_SERIALIZE_ENUM(OperationType,
                             {
                                 {OperationType::kSet, "Set"},
                                 {OperationType::kGet, "Get"},
                                 {OperationType::kSend, "Send"},
                                 {OperationType::kReceive, "Receive"},
                             })

NLOHMANN_JSON_SERIALIZE_ENUM(ObjectType, {
                                             {ObjectType::kPin, "Pin"},
                                             {ObjectType::kUart, "Uart"},
                                             {ObjectType::kI2C, "I2C"},
                                         })

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PinEmulatorRequest, type, object, name,
                                   operation, state)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PinEmulatorResponse, type, object, name,
                                   state, status)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UartEmulatorRequest, type, object, name,
                                   operation, data, size, timeout_ms)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UartEmulatorResponse, type, object, name,
                                   data, bytes_transferred, status)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(I2CEmulatorRequest, type, object, name,
                                   operation, address, data, size)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(I2CEmulatorResponse, type, object, name,
                                   address, data, bytes_transferred, status)

template <typename T>
inline auto Encode(const T& obj) -> std::string {
  return nlohmann::json(obj).dump();
};

template <typename T>
inline auto Decode(const std::string_view& str) -> T {
  return nlohmann::json::parse(str).get<T>();
};

}  // namespace mcu
