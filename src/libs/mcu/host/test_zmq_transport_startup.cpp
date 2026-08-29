// Startup-failure tests for ZmqTransport.
//
// Every test here targets a path that used to hang the constructor forever
// rather than return: bind_cv_.wait() had no deadline, so any failure that
// stopped the server thread before it published a bind outcome parked the
// caller permanently. They live in their own binary because the watchdog below
// hard-exits the process on a regression, which must not take unrelated tests
// with it.

#include <gtest/gtest.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
#include <zmq.hpp>

#include "libs/common/error.hpp"
#include "libs/common/logger.hpp"
#include "libs/mcu/host/dispatcher.hpp"
#include "libs/mcu/host/receiver.hpp"
#include "libs/mcu/host/zmq_transport.hpp"

namespace {

using CreateResult =
    std::expected<std::unique_ptr<mcu::ZmqTransport>, common::Error>;

constexpr auto kWatchdogBudget = std::chrono::seconds{10};
constexpr auto kStartupTimeout = std::chrono::milliseconds{2000};

// Collects log output so a test can assert on the reason for a failure, not
// merely that one occurred. The mutex is load-bearing: the bind phase logs from
// the server thread while the test thread is still inside the constructor.
class RecordingLogger : public common::Logger {
 public:
  auto Debug(std::string_view msg) -> void override { Record(msg); }
  auto Info(std::string_view msg) -> void override { Record(msg); }
  auto Warning(std::string_view msg) -> void override { Record(msg); }
  auto Error(std::string_view msg) -> void override { Record(msg); }

  auto Contains(std::string_view needle) const -> bool {
    const std::lock_guard<std::mutex> lock(mutex_);
    return std::ranges::any_of(messages_, [needle](const std::string& msg) {
      return msg.find(needle) != std::string::npos;
    });
  }

 private:
  auto Record(std::string_view msg) -> void {
    const std::lock_guard<std::mutex> lock(mutex_);
    messages_.emplace_back(msg);
  }

  mutable std::mutex mutex_;
  std::vector<std::string> messages_;
};

// Answers anything, so a test can prove a message reached a given transport's
// dispatcher rather than some other process that stole the endpoint.
class EchoReceiver : public mcu::Receiver {
 public:
  auto Receive(std::string_view message)
      -> std::expected<std::string, common::Error> override {
    return std::string{"echo:"} + std::string{message};
  }
};

// Endpoints are per-test and per-process so a stuck or slow test can never
// collide with another, and never with the emulator's real endpoints.
auto UniqueEndpoint(std::string_view suffix) -> std::string {
  const auto* const info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return std::string{"ipc:///tmp/zt_startup_"} + info->name() + "_" +
         std::to_string(::getpid()) + "_" + std::string{suffix} + ".ipc";
}

auto PathOf(const std::string& endpoint) -> std::string {
  return endpoint.substr(std::string_view{"ipc://"}.size());
}

auto MakeConfig(common::Logger& logger) -> mcu::TransportConfig {
  // TransportConfig has user-provided constructors, so it is not an aggregate:
  // designated initialisers will not compile. Assign after construction.
  mcu::TransportConfig config{logger};
  config.startup_timeout = kStartupTimeout;
  return config;
}

// Runs `fn` with a hard time budget.
//
// The hang being guarded against is inside a constructor called on this thread,
// so bounding it needs a second thread. std::async is unusable: its future's
// destructor joins, so a stuck task would hang the test at scope exit anyway.
// A detached thread over a shared_ptr-owned packaged_task lets the stuck thread
// outlive the call without dangling.
auto RunWithWatchdog(std::function<CreateResult()> action)
    -> std::optional<CreateResult> {
  auto task =
      std::make_shared<std::packaged_task<CreateResult()>>(std::move(action));
  auto future = task->get_future();
  std::thread{[task]() { (*task)(); }}.detach();

  if (future.wait_for(kWatchdogBudget) != std::future_status::ready) {
    return std::nullopt;
  }
  return future.get();
}

// Sends one message to `endpoint` from a fresh peer and returns whatever came
// back, or nullopt if the exchange did not complete. Extracted from the test
// body to keep its cognitive complexity under the clang-tidy threshold.
auto RoundTripThroughEndpoint(const std::string& endpoint)
    -> std::optional<std::string> {
  zmq::context_t context{1};
  zmq::socket_t peer{context, zmq::socket_type::pair};
  peer.set(zmq::sockopt::linger, 0);
  peer.set(zmq::sockopt::sndtimeo, 2000);
  peer.set(zmq::sockopt::rcvtimeo, 2000);
  peer.connect(endpoint);

  std::optional<std::string> reply_text{};
  if (peer.send(zmq::str_buffer("ping"), zmq::send_flags::none)) {
    zmq::message_t reply{};
    if (peer.recv(reply, zmq::recv_flags::none)) {
      reply_text = reply.to_string();
    }
  }

  peer.close();
  context.close();
  return reply_text;
}

// A wedged thread cannot be unwound, and letting it linger through static
// destruction of live ZMQ contexts crashes unpredictably. Exit loudly instead:
// ctest sees a non-zero status, and the ADD_FAILURE line names the regression.
[[noreturn]] auto AbortOnHang() -> void {
  ADD_FAILURE() << "ZmqTransport::Create() never returned within "
                << kWatchdogBudget.count()
                << "s -- the startup hang has regressed";
  std::cout << std::flush;
  std::_Exit(EXIT_FAILURE);
}

// Leaves behind exactly what a SIGKILLed process leaves: a bound socket file
// with no listener. Closing without unlinking is the whole point.
auto LeaveStaleSocketFile(const std::string& path) -> void {
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  ASSERT_LT(path.size(), sizeof(address.sun_path));
  path.copy(static_cast<char*>(address.sun_path), path.size());

  const int descriptor = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  ASSERT_GE(descriptor, 0);
  const auto* const address_ptr = reinterpret_cast<const sockaddr*>(&address);
  ASSERT_EQ(::bind(descriptor, address_ptr, sizeof(address)), 0);
  ::close(descriptor);
}

// Forks kContenders processes that all attempt to bind `contested` at the same
// instant and returns how many believed they succeeded.
//
// The shared spin barrier is the point. Launching processes normally spreads
// their arrivals over milliseconds, so the first one binds and the rest see a
// live owner -- the window never opens and the race looks absent. Releasing
// them together from one store contends it properly.
//
// Lives outside the test body to keep TestBody's cognitive complexity under the
// clang-tidy threshold.
auto CountConcurrentBindWinners(const std::string& contested) -> int {
  constexpr int kContenders = 12;

  auto* gate = static_cast<std::atomic<int>*>(
      ::mmap(nullptr, sizeof(std::atomic<int>), PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  if (gate == MAP_FAILED) {
    return -1;
  }
  gate->store(0);

  for (int i = 0; i < kContenders; ++i) {
    if (::fork() != 0) {
      continue;  // Parent.
    }
    // Child. Exits via _exit so it never runs gtest teardown or atexit handlers
    // belonging to the parent's test process.
    common::NullLogger logger;
    mcu::TransportConfig config{logger};
    config.startup_timeout = kStartupTimeout;
    const mcu::ReceiverMap receivers{};
    mcu::Dispatcher dispatcher{receivers};
    const std::string own = contested + ".peer" + std::to_string(i);

    while (gate->load(std::memory_order_acquire) == 0) {
      // Spin rather than sleep: the point is to arrive together.
    }
    auto transport =
        mcu::ZmqTransport::Create(own, contested, dispatcher, config);
    const bool won = transport.has_value();
    if (won) {
      // Hold it briefly so later arrivals genuinely contend.
      std::this_thread::sleep_for(std::chrono::milliseconds{400});
    }
    ::_exit(won ? 0 : 1);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds{250});  // all spinning
  gate->store(1, std::memory_order_release);                    // release

  int winners = 0;
  for (int i = 0; i < kContenders; ++i) {
    int status = 0;
    ::wait(&status);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      ++winners;
    }
  }
  ::munmap(gate, sizeof(std::atomic<int>));

  std::error_code error{};
  for (int i = 0; i < kContenders; ++i) {
    const std::string own = PathOf(contested) + ".peer" + std::to_string(i);
    std::filesystem::remove(own, error);
    std::filesystem::remove(own + ".lock", error);
  }
  return winners;
}

class ZmqTransportStartupTest : public ::testing::Test {
 protected:
  void TearDown() override {
    std::error_code error{};
    for (const auto& endpoint : cleanup_) {
      std::filesystem::remove(PathOf(endpoint), error);
      std::filesystem::remove(PathOf(endpoint) + ".lock", error);
    }
  }

  auto TrackForCleanup(const std::string& endpoint) -> std::string {
    cleanup_.push_back(endpoint);
    return endpoint;
  }

  RecordingLogger logger_;
  const mcu::ReceiverMap empty_receivers_;

 private:
  std::vector<std::string> cleanup_;
};

// A bind that cannot succeed must be reported, not waited out. The elapsed
// assertion is the real subject: a kOperationFailed that took startup_timeout
// would mean the outcome was never published and we merely timed out.
TEST_F(ZmqTransportStartupTest, CreateFailsFastWhenBindEndpointIsUnbindable) {
  mcu::Dispatcher dispatcher{empty_receivers_};
  auto config = MakeConfig(logger_);
  const auto to_endpoint = TrackForCleanup(UniqueEndpoint("to"));
  const std::string from_endpoint{
      "ipc:///tmp/zt_startup_no_such_directory/from.ipc"};

  const auto start = std::chrono::steady_clock::now();
  const auto outcome = RunWithWatchdog([&]() {
    return mcu::ZmqTransport::Create(to_endpoint, from_endpoint, dispatcher,
                                     config);
  });
  const auto elapsed = std::chrono::steady_clock::now() - start;

  if (!outcome) {
    AbortOnHang();
  }
  ASSERT_FALSE(outcome->has_value());
  EXPECT_EQ(outcome->error(), common::Error::kOperationFailed);
  EXPECT_LT(elapsed, kStartupTimeout)
      << "reported rather than timed out is the point";
  EXPECT_TRUE(logger_.Contains("ServerThread failed to bind"));
}

// libzmq unlinks an ipc path before binding it, so it will displace a live
// listener and take its name with no error on either side. Refusing to start is
// the only way to keep the first owner's endpoint intact.
TEST_F(ZmqTransportStartupTest, CreateRefusesToStealEndpointFromLiveOwner) {
  EchoReceiver echo;
  const mcu::ReceiverMap receivers{std::ref(echo)};
  mcu::Dispatcher owner_dispatcher{receivers};
  auto owner_config = MakeConfig(logger_);

  const auto owner_to = TrackForCleanup(UniqueEndpoint("owner_to"));
  const auto contested = TrackForCleanup(UniqueEndpoint("contested"));

  auto owner = mcu::ZmqTransport::Create(owner_to, contested, owner_dispatcher,
                                         owner_config);
  ASSERT_TRUE(owner.has_value());

  RecordingLogger thief_logger;
  mcu::Dispatcher thief_dispatcher{empty_receivers_};
  auto thief_config = MakeConfig(thief_logger);
  const auto thief_to = TrackForCleanup(UniqueEndpoint("thief_to"));

  const auto outcome = RunWithWatchdog([&]() {
    return mcu::ZmqTransport::Create(thief_to, contested, thief_dispatcher,
                                     thief_config);
  });

  if (!outcome) {
    AbortOnHang();
  }
  ASSERT_FALSE(outcome->has_value());
  EXPECT_EQ(outcome->error(), common::Error::kOperationFailed);
  // Either guard is a correct refusal, and which one fires is an implementation
  // detail: EndpointLock runs first and will normally catch it, with the
  // connect(2) probe behind it for an owner that holds no lock.
  EXPECT_TRUE(thief_logger.Contains("Refusing to bind"));

  // The assertion that actually proves no hijack occurred: a fresh peer
  // connecting to the contested endpoint still reaches the original owner.
  EXPECT_EQ(RoundTripThroughEndpoint(contested),
            std::optional<std::string>{"echo:ping"});
}

// The mirror image, and unlike its two neighbours this is NOT a regression
// test -- it passes on the pre-change code too, because libzmq's own unlink
// always made stale files a non-problem. It guards the liveness probe added
// alongside it: an over-eager probe that read "file exists" as "owned" would
// refuse to start after any crash, turning a harmless leftover into a failure.
TEST_F(ZmqTransportStartupTest, CreateSucceedsOverStaleSocketFile) {
  mcu::Dispatcher dispatcher{empty_receivers_};
  auto config = MakeConfig(logger_);
  const auto to_endpoint = TrackForCleanup(UniqueEndpoint("to"));
  const auto from_endpoint = TrackForCleanup(UniqueEndpoint("from"));

  LeaveStaleSocketFile(PathOf(from_endpoint));
  ASSERT_TRUE(std::filesystem::exists(PathOf(from_endpoint)));

  const auto outcome = RunWithWatchdog([&]() {
    return mcu::ZmqTransport::Create(to_endpoint, from_endpoint, dispatcher,
                                     config);
  });

  if (!outcome) {
    AbortOnHang();
  }
  ASSERT_TRUE(outcome->has_value());
  EXPECT_TRUE(outcome->value()->IsReady());
}

// Twelve processes contend for one bind endpoint, released together by a shared
// spin barrier so the decide-then-bind window is genuinely contended rather
// than spread out by process startup.
//
// Exactly one may win. Every additional winner is a process that believes it
// owns an endpoint libzmq has already unlinked out from under it -- they do not
// fail, which is precisely what makes this worth a test.
//
// Measured with EndpointLock disabled and only the connect(2) probe in place,
// this produces between 1 and 7 winners per run. That spread is the reason the
// lock exists: a probe followed by a bind is two syscalls with a window between
// them, and flock has no window at all.
TEST_F(ZmqTransportStartupTest, ConcurrentBindsProduceExactlyOneOwner) {
  const auto contested = TrackForCleanup(UniqueEndpoint("contested"));
  const int winners = CountConcurrentBindWinners(contested);
  EXPECT_EQ(winners, 1) << winners << " processes each believe they own "
                        << contested;
}

}  // namespace
