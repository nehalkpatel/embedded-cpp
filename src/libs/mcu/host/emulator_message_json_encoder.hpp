#pragma once

#include <cstddef>
#include <expected>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "libs/common/error.hpp"
#include "libs/mcu/host/host_emulator_messages.hpp"
#include "libs/mcu/host/transport.hpp"
#include "libs/mcu/pin.hpp"

// Custom JSON serialization for std::byte. The function names and signatures
// are nlohmann's ADL contract, so the project naming convention does not apply.
namespace nlohmann {
template <>
struct adl_serializer<std::byte> {
  // NOLINTNEXTLINE(readability-identifier-naming)
  static void to_json(json& encoded, const std::byte& value) {
    encoded = std::to_integer<uint8_t>(value);
  }

  // NOLINTNEXTLINE(readability-identifier-naming)
  static void from_json(const json& encoded, std::byte& value) {
    value = static_cast<std::byte>(encoded.get<uint8_t>());
  }
};
}  // namespace nlohmann

// The NLOHMANN_* macros below expand to code that cannot satisfy the project's
// clang-tidy checks (short names, C arrays, pre-C++17 type traits); suppress
// those checks for the macro expansions only.
// NOLINTBEGIN(readability-identifier-length)
// NOLINTBEGIN(modernize-avoid-c-arrays)
// NOLINTBEGIN(modernize-type-traits)

namespace common {

// Covers every common::Error enumerator (error.hpp). When adding an
// enumerator, add its wire name here and in the Python emulator's Status enum
// (py/host-emulator/src/host_emulator/common.py) — an enumerator missing from
// this table serializes as null and decodes as the first entry.
NLOHMANN_JSON_SERIALIZE_ENUM(Error,
                             {
                                 {Error::kOk, "Ok"},
                                 {Error::kUnknown, "Unknown"},
                                 {Error::kInvalidArgument, "InvalidArgument"},
                                 {Error::kInvalidState, "InvalidState"},
                                 {Error::kInvalidOperation, "InvalidOperation"},
                                 {Error::kOperationFailed, "OperationFailed"},
                                 {Error::kUnhandled, "Unhandled"},
                                 {Error::kConnectionRefused,
                                  "ConnectionRefused"},
                                 {Error::kConnectionClosed, "ConnectionClosed"},
                                 {Error::kTimeout, "Timeout"},
                                 {Error::kWouldBlock, "WouldBlock"},
                                 {Error::kMessageTooLarge, "MessageTooLarge"},
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

// NOLINTEND(modernize-type-traits)
// NOLINTEND(modernize-avoid-c-arrays)
// NOLINTEND(readability-identifier-length)

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
auto Encode(const T& obj) -> std::string {
  return nlohmann::json(obj).dump();
}

template <typename T>
auto Decode(std::string_view str) -> std::expected<T, common::Error> {
  const auto parsed =
      nlohmann::json::parse(str, nullptr, /*allow_exceptions=*/false);
  if (parsed.is_discarded()) {
    return std::unexpected(common::Error::kInvalidArgument);
  }
  // get<T>() still throws on a structural mismatch (missing field, wrong
  // type); this is the one place the nlohmann boundary needs a catch in this
  // otherwise exception-free codebase.
  try {
    return parsed.template get<T>();
  } catch (const nlohmann::json::exception&) {
    return std::unexpected(common::Error::kInvalidArgument);
  }
}

/// One request/response exchange with the emulator: encode the request, send
/// it, wait for the reply, decode it, and fold the emulator's status field
/// into the error channel. Every host peripheral funnels its blocking
/// operations through here — it is also the codebase's reference example of
/// chaining std::expected with and_then.
template <typename Response, typename Request>
auto Transact(Transport& transport, const Request& request)
    -> std::expected<Response, common::Error> {
  return transport.Send(Encode(request))
      .and_then([&transport]() { return transport.Receive(); })
      .and_then(
          [](const std::string& reply) { return Decode<Response>(reply); })
      .and_then(
          [](Response&& response) -> std::expected<Response, common::Error> {
            if (response.status != common::Error::kOk) {
              return std::unexpected(response.status);
            }
            return std::move(response);
          });
}

}  // namespace mcu
