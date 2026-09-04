/// @file
/// The newlib syscalls this board provides, replacing --specs=nosys.specs.
///
/// newlib-nano expects a small set of POSIX-shaped functions to exist. Most of
/// them have no meaning on a board with no filesystem and no processes, and
/// exist only so the C library links; the two that do real work are _write,
/// which reaches the console USART, and _sbrk, which hands out the heap.
///
/// A heap is not optional here even though nothing in this project calls new:
/// a polymorphic class's vtable references its deleting destructor, which
/// references operator delete, which pulls in free and newlib's malloc arena.
/// Some applications do allocate -- uart_echo builds a std::vector in its
/// receive handler and a std::string for its greeting.

#include <cerrno>
#include <cstddef>
#include <cstdint>

#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/usart.hpp"

namespace {

// Defined by the linker script (stm32f767zi.ld). `end` is the first address
// past .bss; __heap_limit leaves the stack its reserved space at the top of
// RAM. Declared as arrays so their *addresses* are the values -- a linker
// symbol has no storage to load from.
extern "C" char end[];           // NOLINT(modernize-avoid-c-arrays)
extern "C" char __heap_limit[];  // NOLINT(modernize-avoid-c-arrays)

char* g_heap_break = nullptr;

// Guards newlib's allocator against re-entry from an interrupt handler; see
// __malloc_lock below.
std::uint32_t g_malloc_lock_depth = 0;
std::uint32_t g_saved_primask = 0;

}  // namespace

extern "C" {

/// Grow the heap. Bounded, unlike the usual vendor implementation: exceeding
/// the limit returns an allocation failure rather than quietly handing out
/// addresses that the stack is about to grow down into, which corrupts memory
/// far from the code responsible and long after the fact.
auto _sbrk(ptrdiff_t increment) -> void* {
  if (g_heap_break == nullptr) {
    g_heap_break = static_cast<char*>(end);
  }

  char* const previous = g_heap_break;
  if (increment > 0 && (__heap_limit - g_heap_break) < increment) {
    errno = ENOMEM;
    return reinterpret_cast<void*>(-1);  // NOLINT(performance-no-int-to-ptr)
  }
  g_heap_break += increment;
  return previous;
}

/// stdout and stderr go to the console USART; anything else is a bad file
/// descriptor. Newlines are expanded to CRLF because terminal emulators on the
/// other end of the virtual COM port expect it.
auto _write(int file, const char* data, int length) -> int {
  if (file != 1 && file != 2) {
    errno = EBADF;
    return -1;
  }
  for (int i = 0; i < length; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char value = data[i];
    if (value == '\n') {
      static_cast<void>(mcu::PutcharToConsole('\r'));
    }
    if (!mcu::PutcharToConsole(value)) {
      // No console yet. Report the write as complete rather than failing:
      // output before Uart1().Init() is diagnostics, and a failing write there
      // turns startup logging into an error path.
      return length;
    }
  }
  return length;
}

// No filesystem. These exist so newlib links; every one reports the honest
// answer for a target with no files, rather than pretending to succeed.
auto _read(int /*file*/, char* /*data*/, int /*length*/) -> int { return 0; }
auto _close(int /*file*/) -> int { return -1; }
auto _lseek(int /*file*/, int /*offset*/, int /*whence*/) -> int { return 0; }

/// Claiming character-device status keeps newlib's stdout unbuffered, which is
/// what you want when the next thing after a printf may be a hard fault.
auto _isatty(int /*file*/) -> int { return 1; }

auto _fstat(int /*file*/, struct stat* /*st*/) -> int {
  return 0;  // st_mode is left alone; _isatty is what newlib actually consults.
}

auto _getpid() -> int { return 1; }

auto _kill(int /*pid*/, int /*sig*/) -> int {
  errno = EINVAL;
  return -1;
}

/// Nothing to exit to. Stop with interrupts off, so a debugger attaches to a
/// halted core rather than one still servicing timers.
[[noreturn]] auto _exit(int /*status*/) -> void {
  __disable_irq();
  while (true) {
  }
}

/// newlib-nano ships __malloc_lock and __malloc_unlock as `bx lr` -- correct
/// for a single-threaded program, and wrong the moment an interrupt handler
/// allocates. uart_echo's receive handler builds a std::vector, so a USART
/// interrupt arriving while the main context is inside malloc would re-enter
/// the allocator and corrupt its arena: a fault later, somewhere unrelated.
///
/// Masking interrupts around the allocator is the standard fix and costs
/// nothing when uncontended. Nesting is counted, and the previous PRIMASK
/// restored rather than assumed clear, so a call from a context that already
/// had interrupts disabled does not silently enable them on the way out.
auto __malloc_lock(struct _reent* /*reent*/) -> void {
  const std::uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (g_malloc_lock_depth == 0) {
    g_saved_primask = primask;
  }
  ++g_malloc_lock_depth;
}

auto __malloc_unlock(struct _reent* /*reent*/) -> void {
  if (g_malloc_lock_depth > 0) {
    --g_malloc_lock_depth;
  }
  if (g_malloc_lock_depth == 0 && (g_saved_primask & 1U) == 0U) {
    __enable_irq();
  }
}

/// Called when a pure virtual is invoked -- during construction or after
/// destruction of a base. newlib's default drags in std::terminate's
/// machinery; this is smaller and stops at a known address.
[[noreturn]] auto __cxa_pure_virtual() -> void {
  __disable_irq();
  while (true) {
  }
}

}  // extern "C"
