#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "libs/common/error.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

enum class MessageType { kRequest = 1, kResponse };
enum class OperationType { kSet = 1, kGet, kSend, kReceive };
enum class ObjectType { kPin = 1, kUart };

struct PinEmulatorRequest {
  MessageType type{MessageType::kRequest};
  ObjectType object{ObjectType::kPin};
  std::string name;
  OperationType operation;
  PinState state;
  auto operator==(const PinEmulatorRequest& other) const -> bool {
    return type == other.type && object == other.object && name == other.name &&
           operation == other.operation && state == other.state;
  }
};

struct PinEmulatorResponse {
  MessageType type{MessageType::kResponse};
  ObjectType object{ObjectType::kPin};
  std::string name;
  PinState state;
  common::Error status;
  auto operator==(const PinEmulatorResponse& other) const -> bool {
    return type == other.type && object == other.object && name == other.name &&
           state == other.state && status == other.status;
  }
};

struct UartEmulatorRequest {
  MessageType type{MessageType::kRequest};
  ObjectType object{ObjectType::kUart};
  std::string name;
  OperationType operation;
  std::vector<uint8_t> data;  // For Send operation
  size_t size{0};             // For Receive operation (buffer size)
  uint32_t timeout_ms{0};     // For Receive operation
  auto operator==(const UartEmulatorRequest& other) const -> bool {
    return type == other.type && object == other.object && name == other.name &&
           operation == other.operation && data == other.data &&
           size == other.size && timeout_ms == other.timeout_ms;
  }
};

struct UartEmulatorResponse {
  MessageType type{MessageType::kResponse};
  ObjectType object{ObjectType::kUart};
  std::string name;
  std::vector<uint8_t> data;  // Received data
  size_t bytes_transferred{0};
  common::Error status;
  auto operator==(const UartEmulatorResponse& other) const -> bool {
    return type == other.type && object == other.object && name == other.name &&
           data == other.data && bytes_transferred == other.bytes_transferred &&
           status == other.status;
  }
};

}  // namespace mcu
