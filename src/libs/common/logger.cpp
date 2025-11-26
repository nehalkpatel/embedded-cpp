#include "logger.hpp"

#include <iostream>

namespace common {

auto ConsoleLogger::Debug(std::string_view msg) -> void {
  std::cout << "[DEBUG] " << msg << '\n';
}

auto ConsoleLogger::Info(std::string_view msg) -> void {
  std::cout << "[INFO]  " << msg << '\n';
}

auto ConsoleLogger::Warning(std::string_view msg) -> void {
  std::cerr << "[WARN]  " << msg << '\n';
}

auto ConsoleLogger::Error(std::string_view msg) -> void {
  std::cerr << "[ERROR] " << msg << '\n';
}

}  // namespace common
