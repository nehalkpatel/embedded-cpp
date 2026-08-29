#pragma once

#include <string>

namespace mcu {

// Exclusive advisory ownership of a bind endpoint, held for the lifetime of the
// transport that took it.
//
// This is what makes "may I bind here" atomic. A connect(2) liveness probe
// cannot be: another process can bind in the window between the probe and our
// own bind, and libzmq will then unlink whichever socket file it finds. flock
// is arbitrated by the kernel, so that window does not exist.
//
// It is also crash-safe, which an O_EXCL lock file is not: the lock lives on
// the open file description and the kernel drops it when the fd closes --
// including when the process dies -- so a SIGKILLed run leaves nothing behind
// that would block the next one.
class EndpointLock {
 public:
  EndpointLock() = default;
  EndpointLock(const EndpointLock&) = delete;
  EndpointLock(EndpointLock&&) = delete;
  auto operator=(const EndpointLock&) -> EndpointLock& = delete;
  auto operator=(EndpointLock&&) -> EndpointLock& = delete;
  ~EndpointLock();

  // Takes the lock guarding `endpoint`. False means another live process holds
  // it. Endpoints with no lockable path succeed trivially.
  auto TryAcquire(const std::string& endpoint) -> bool;

 private:
  int fd_{-1};
};

// Whether another live process is already serving this ipc:// endpoint.
//
// This is deliberately NOT stale-file cleanup. libzmq unlinks an ipc path
// before binding it, unconditionally, so a file left behind by a crashed
// process is already a non-problem -- bind() simply succeeds.
//
// The same unlink is what makes a *live* owner a problem. libzmq will happily
// remove a path another process is actively listening on and bind its own
// socket in place (verified: a second bind() to a held endpoint succeeds).
// Neither side sees an error. The original owner keeps its existing
// connections, because the inode outlives the name, but every subsequent
// connect() reaches the thief instead -- so a second app instance, or a unit
// test run while the emulator is up, silently splits the bus in two.
//
// libzmq gives us no way to ask it not to do that, so callers check before
// handing it the endpoint and refuse to start rather than become the thief.
//
// On its own this check is racy -- another process can bind between it and the
// caller's bind. EndpointLock closes that window for anything using the same
// lock; this remains as the best available answer for an owner that is not.
auto EndpointHasLiveOwner(const std::string& endpoint) -> bool;

}  // namespace mcu
