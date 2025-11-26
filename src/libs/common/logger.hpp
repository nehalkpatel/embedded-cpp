#pragma once

#include <string_view>

namespace common {

enum class LogLevel { kDebug, kInfo, kWarning, kError };

// Abstract logging interface
class Logger {
 public:
  virtual ~Logger() = default;

  virtual auto Debug(std::string_view msg) -> void = 0;
  virtual auto Info(std::string_view msg) -> void = 0;
  virtual auto Warning(std::string_view msg) -> void = 0;
  virtual auto Error(std::string_view msg) -> void = 0;
};

// Null logger - discards all messages (default for embedded)
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
