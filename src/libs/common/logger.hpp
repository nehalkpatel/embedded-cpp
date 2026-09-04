#pragma once

#include <cstdint>
#include <string_view>

namespace common {

enum class LogLevel : std::uint8_t { kDebug, kInfo, kWarning, kError };

// Abstract logging interface
class Logger {
 public:
  virtual ~Logger() = default;

  virtual auto Debug(std::string_view msg) -> void = 0;
  virtual auto Info(std::string_view msg) -> void = 0;
  virtual auto Warning(std::string_view msg) -> void = 0;
  virtual auto Error(std::string_view msg) -> void = 0;
};

// Null logger - discards all messages (default for embedded).
//
// This is the only Logger a cross build can use. ConsoleLogger's out-of-line
// definitions (logger.cpp) are written against <print>, which libstdc++ 13
// does not provide -- and arm-none-eabi-g++ is 13.2. A hardware target that
// wants real output should retarget newlib's _write to a UART instead of
// reaching for std::println here.
class NullLogger : public Logger {
 public:
  auto Debug(std::string_view /* msg */) -> void override {}
  auto Info(std::string_view /* msg */) -> void override {}
  auto Warning(std::string_view /* msg */) -> void override {}
  auto Error(std::string_view /* msg */) -> void override {}
};

// Console logger - prints to stdout/stderr (useful for host builds)
class ConsoleLogger : public Logger {
 public:
  auto Debug(std::string_view msg) -> void override;
  auto Info(std::string_view msg) -> void override;
  auto Warning(std::string_view msg) -> void override;
  auto Error(std::string_view msg) -> void override;
};

}  // namespace common
