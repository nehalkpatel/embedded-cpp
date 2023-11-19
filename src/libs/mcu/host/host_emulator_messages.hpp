#pragma once

#include <expected>
#include <string>

#include "libs/common/error.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

enum class MessageType { kRequest = 1, kResponse };
enum class OperationType { kSet = 1, kGet };
enum class ObjectType { kPin = 1 };

struct PinEmulatorRequest {
  MessageType type = MessageType::kRequest;
  ObjectType object = ObjectType::kPin;
  std::string name;
  OperationType operation;
  PinState state;
};

struct PinEmulatorResponse {
  MessageType type = MessageType::kResponse;
  ObjectType object = ObjectType::kPin;
  std::string name;
  PinState state;
  common::Error status;
};

}  // namespace mcu
