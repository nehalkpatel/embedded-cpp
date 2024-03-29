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

NLOHMANN_JSON_SERIALIZE_ENUM(OperationType, {
                                                {OperationType::kSet, "Set"},
                                                {OperationType::kGet, "Get"},
                                            })

NLOHMANN_JSON_SERIALIZE_ENUM(ObjectType, {
                                             {ObjectType::kPin, "Pin"},
                                         })

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PinEmulatorRequest, type, object, name,
                                   operation, state)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PinEmulatorResponse, type, object, name,
                                   state, status)

template <typename T>
inline auto Encode(const T& obj) -> std::string {
  return nlohmann::json(obj).dump();
};

template <typename T>
inline auto Decode(const std::string_view& str) -> T {
  return nlohmann::json::parse(str).get<T>();
};

}  // namespace mcu
