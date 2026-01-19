#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "libs/common/error.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

enum class MessageType { kRequest = 1, kResponse };
enum class OperationType { kSet = 1, kGet, kSend, kReceive };
enum class ObjectType { kPin = 1, kUart, kI2C };

struct PinEmulatorRequest {
  MessageType type{MessageType::kRequest};
  ObjectType object{ObjectType::kPin};
  std::string name;
  OperationType operation;
  PinState state;
  auto operator<=>(const PinEmulatorRequest&) const = default;
};

struct PinEmulatorResponse {
  MessageType type{MessageType::kResponse};
  ObjectType object{ObjectType::kPin};
  std::string name;
  PinState state;
  common::Error status;
  auto operator<=>(const PinEmulatorResponse&) const = default;
};

struct UartEmulatorRequest {
  MessageType type{MessageType::kRequest};
  ObjectType object{ObjectType::kUart};
  std::string name;
  OperationType operation;
  std::vector<std::byte> data;  // For Send operation
  size_t size{0};               // For Receive operation (buffer size)
  uint32_t timeout_ms{0};       // For Receive operation
  auto operator<=>(const UartEmulatorRequest&) const = default;
};

struct UartEmulatorResponse {
  MessageType type{MessageType::kResponse};
  ObjectType object{ObjectType::kUart};
  std::string name;
  std::vector<std::byte> data;  // Received data
  size_t bytes_transferred{0};
  common::Error status;
  auto operator<=>(const UartEmulatorResponse&) const = default;
};

struct I2CEmulatorRequest {
  MessageType type{MessageType::kRequest};
  ObjectType object{ObjectType::kI2C};
  std::string name;
  OperationType operation;
  uint16_t address{0};
  std::vector<std::byte> data;  // For Send operation
  size_t size{0};               // For Receive operation (buffer size)
  auto operator<=>(const I2CEmulatorRequest&) const = default;
};

struct I2CEmulatorResponse {
  MessageType type{MessageType::kResponse};
  ObjectType object{ObjectType::kI2C};
  std::string name;
  uint16_t address{0};
  std::vector<std::byte> data;  // Received data
  size_t bytes_transferred{0};
  common::Error status;
  auto operator<=>(const I2CEmulatorResponse&) const = default;
};

}  // namespace mcu
