#include "endpoint_lock.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace mcu {
namespace {

constexpr std::string_view kIpcScheme{"ipc://"};
constexpr std::string_view kLockSuffix{".lock"};
constexpr mode_t kLockFileMode{0600};

// RAII for a bare file descriptor. The liveness probe below is the only place
// this file talks to POSIX sockets directly, and it must not leak an fd on any
// of its several early returns.
class FdGuard {
 public:
  explicit FdGuard(int descriptor) : fd_{descriptor} {}
  FdGuard(const FdGuard&) = delete;
  FdGuard(FdGuard&&) = delete;
  auto operator=(const FdGuard&) -> FdGuard& = delete;
  auto operator=(FdGuard&&) -> FdGuard& = delete;
  ~FdGuard() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  [[nodiscard]] auto Get() const -> int { return fd_; }

 private:
  int fd_;
};

// True if some process is currently accepting on the AF_UNIX socket at `path`.
//
// libzmq's ipc:// transport is AF_UNIX/SOCK_STREAM, so a plain connect(2) is a
// valid liveness probe with no ZMQ machinery involved: a path left behind by a
// killed process refuses the connection, a live listener accepts it.
//
// Every "cannot tell" answer is reported as live, so the caller never proceeds
// past something it does not understand. Refusing to start is recoverable; the
// hijack described in EndpointHasLiveOwner is not.
auto IpcPathHasLiveOwner(const std::string& path) -> bool {
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    return true;  // Too long to probe; assume live rather than guess.
  }
  path.copy(static_cast<char*>(address.sun_path), path.size());

  const FdGuard probe{::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
  if (probe.Get() < 0) {
    return true;
  }

  const auto* const address_ptr = reinterpret_cast<const sockaddr*>(&address);
  if (::connect(probe.Get(), address_ptr, sizeof(address)) == 0) {
    return true;  // Someone is listening -- hands off.
  }
  return errno != ECONNREFUSED;  // ECONNREFUSED means the owner is gone.
}

}  // namespace

EndpointLock::~EndpointLock() {
  if (fd_ >= 0) {
    ::close(fd_);  // Closing the fd is what releases the lock.
  }
}

auto EndpointLock::TryAcquire(const std::string& endpoint) -> bool {
  const std::string_view endpoint_view{endpoint};
  if (!endpoint_view.starts_with(kIpcScheme)) {
    return true;  // No filesystem path to guard.
  }
  const std::string lock_path{
      std::string{endpoint_view.substr(kIpcScheme.size())} +
      std::string{kLockSuffix}};

  // The lock file is deliberately never unlinked. Removing it would reintroduce
  // exactly the race it exists to close: one process unlinking the file another
  // has already opened, leaving the two holding locks on different inodes and
  // both believing they won. It stays behind as a zero-byte marker.
  const int descriptor =
      ::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, kLockFileMode);
  if (descriptor < 0) {
    // Cannot lock here -- a read-only directory, for instance. Fall through to
    // the liveness probe rather than refusing to start over a missing luxury.
    return true;
  }
  if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
    ::close(descriptor);
    return false;
  }
  fd_ = descriptor;
  return true;
}

auto EndpointHasLiveOwner(const std::string& endpoint) -> bool {
  const std::string_view endpoint_view{endpoint};
  if (!endpoint_view.starts_with(kIpcScheme)) {
    return false;  // Only ipc:// is probeable this way.
  }
  const std::string path{endpoint_view.substr(kIpcScheme.size())};

  std::error_code error{};
  if (!std::filesystem::exists(path, error) || error) {
    return false;  // Nothing there at all.
  }
  if (!std::filesystem::is_socket(path, error) || error) {
    return true;  // Not ours to reason about; do not bind over it.
  }
  return IpcPathHasLiveOwner(path);
}

}  // namespace mcu
