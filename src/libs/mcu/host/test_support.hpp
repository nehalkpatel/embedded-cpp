#pragma once

// Shared helpers for the host-transport and host-peripheral tests. Test-only:
// this header is not part of any library's file set.

#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "libs/common/logger.hpp"

namespace mcu::test {

// Collects log output so a test can assert on the reason for a failure — or on
// how often something happened (e.g. how many times Send() went round its
// retry loop) — not merely that a failure occurred. The mutex is load-bearing:
// the transport logs from its server thread while the test thread is acting.
class RecordingLogger : public common::Logger {
 public:
  auto Debug(std::string_view msg) -> void override { Record(msg); }
  auto Info(std::string_view msg) -> void override { Record(msg); }
  auto Warning(std::string_view msg) -> void override { Record(msg); }
  auto Error(std::string_view msg) -> void override { Record(msg); }

  auto Count(std::string_view needle) const -> std::size_t {
    const std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<std::size_t>(
        std::ranges::count_if(messages_, [needle](const std::string& msg) {
          return msg.find(needle) != std::string::npos;
        }));
  }

  auto Contains(std::string_view needle) const -> bool {
    return Count(needle) > 0;
  }

 private:
  auto Record(std::string_view msg) -> void {
    const std::lock_guard<std::mutex> lock(mutex_);
    messages_.emplace_back(msg);
  }

  mutable std::mutex mutex_;
  std::vector<std::string> messages_;
};

// Per-process endpoints. gtest_discover_tests gives every case its own
// process, so a fixed path would make `ctest -j` cases contend for one
// endpoint — and a fixed path shared with the real emulator's defaults would
// have unit tests fighting a live emulator session.
inline auto MakeEndpoint(std::string_view prefix,
                         std::string_view role) -> std::string {
  return "ipc:///tmp/" + std::string{prefix} + "_" + std::string{role} + "_" +
         std::to_string(::getpid()) + ".ipc";
}

inline auto EndpointPath(std::string_view endpoint) -> std::string {
  return std::string{endpoint.substr(std::string_view{"ipc://"}.size())};
}

// The transport never unlinks its own lock file — doing so would reopen the
// race it closes — so per-process test endpoints would otherwise pile up in
// /tmp, one socket + .lock pair per test case per run.
inline auto RemoveEndpointArtifacts(std::string_view endpoint) -> void {
  const std::string path{EndpointPath(endpoint)};
  std::error_code error{};
  std::filesystem::remove(path, error);
  std::filesystem::remove(path + ".lock", error);
}

}  // namespace mcu::test
