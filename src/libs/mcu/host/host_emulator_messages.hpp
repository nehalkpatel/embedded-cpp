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
  bool operator==(const PinEmulatorRequest& other) const {
    return type == other.type && object == other.object && name == other.name &&
           operation == other.operation && state == other.state;
  }
};

struct PinEmulatorResponse {
  MessageType type = MessageType::kResponse;
  ObjectType object = ObjectType::kPin;
  std::string name;
  PinState state;
  common::Error status;
  bool operator==(const PinEmulatorResponse& other) const {
    return type == other.type && object == other.object && name == other.name &&
           state == other.state && status == other.status;
  }
};

}  // namespace mcu
