#include "logger.hpp"

#include <print>

namespace common {

auto ConsoleLogger::Debug(std::string_view msg) -> void {
  std::println("[DEBUG] {}", msg);
}

auto ConsoleLogger::Info(std::string_view msg) -> void {
  std::println("[INFO]  {}", msg);
}

auto ConsoleLogger::Warning(std::string_view msg) -> void {
  std::println(stderr, "[WARN]  {}", msg);
}

auto ConsoleLogger::Error(std::string_view msg) -> void {
  std::println(stderr, "[ERROR] {}", msg);
}

}  // namespace common
