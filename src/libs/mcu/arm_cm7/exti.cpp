#include "libs/mcu/arm_cm7/exti.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <utility>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/pin.hpp"

namespace mcu {

namespace {

constexpr std::uint32_t kExtiLineCount = 16;

struct ExtiLine {
  std::function<void()> handler;
  std::uint32_t port_index = 0;
  bool claimed = false;
};

// One slot per line, indexed by pin number. Namespace scope rather than
// function-local: an interrupt can arrive before any function-local static
// would have been initialized.
std::array<ExtiLine, kExtiLineCount> g_lines;

/// Route line `pin` to `port_index` via SYSCFG. Each EXTICR word holds four
/// 4-bit fields, so line n lives in word n/4 at bit offset (n%4)*4.
auto SelectPortForLine(std::uint32_t pin, std::uint32_t port_index) -> void {
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  static_cast<void>(RCC->APB2ENR);

  const std::uint32_t word = pin / 4U;
  const std::uint32_t shift = (pin % 4U) * 4U;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  auto value = SYSCFG->EXTICR[word];
  value &= ~(0xFU << shift);
  value |= port_index << shift;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
  SYSCFG->EXTICR[word] = value;
}

/// Lines 0-4 have their own NVIC vectors; 5-9 and 10-15 are each shared.
[[nodiscard]] auto IrqForLine(std::uint32_t pin) -> IRQn_Type {
  if (pin <= 4U) {
    return static_cast<IRQn_Type>(EXTI0_IRQn + static_cast<int>(pin));
  }
  return pin <= 9U ? EXTI9_5_IRQn : EXTI15_10_IRQn;
}

/// Clear the pending flag first, then dispatch. Clearing after the handler
/// would drop an edge that arrived while the handler was running; clearing
/// first at worst runs the handler twice, which is the recoverable direction.
auto ServiceLine(std::uint32_t pin) -> void {
  const std::uint32_t mask = 1U << pin;
  if ((EXTI->PR & mask) == 0U) {
    return;
  }
  EXTI->PR = mask;  // rc_w1: writing 1 clears.

  const auto& line = g_lines.at(pin);
  if (line.handler) {
    line.handler();
  }
}

auto ServiceRange(std::uint32_t first, std::uint32_t last) -> void {
  for (std::uint32_t pin = first; pin <= last; ++pin) {
    ServiceLine(pin);
  }
}

}  // namespace

auto RegisterExtiHandler(
    GpioPort port, std::uint32_t pin, std::function<void()> handler,
    PinTransition transition) -> std::expected<void, common::Error> {
  if (pin >= kExtiLineCount) {
    return std::unexpected(common::Error::kInvalidArgument);
  }

  const auto port_index = static_cast<std::uint32_t>(port);
  auto& line = g_lines.at(pin);
  if (line.claimed && line.port_index != port_index) {
    // Another port already owns this line. Rerouting it would silently stop
    // delivering the first port's interrupts.
    return std::unexpected(common::Error::kInvalidState);
  }

  line.handler = std::move(handler);
  line.port_index = port_index;
  line.claimed = true;

  SelectPortForLine(pin, port_index);

  const std::uint32_t mask = 1U << pin;
  const bool rising = transition == PinTransition::kRising ||
                      transition == PinTransition::kBoth;
  const bool falling = transition == PinTransition::kFalling ||
                       transition == PinTransition::kBoth;

  if (rising) {
    EXTI->RTSR |= mask;
  } else {
    EXTI->RTSR &= ~mask;
  }
  if (falling) {
    EXTI->FTSR |= mask;
  } else {
    EXTI->FTSR &= ~mask;
  }

  EXTI->PR = mask;  // Discard anything latched during configuration.
  EXTI->IMR |= mask;

  const auto irq = IrqForLine(pin);
  NVIC_SetPriority(irq, 5);
  NVIC_EnableIRQ(irq);

  return {};
}

// The vector table (startup.s) declares these weak and aliased to
// Default_Handler; defining them here overrides the alias.
extern "C" auto EXTI0_IRQHandler() -> void { ServiceLine(0); }
extern "C" auto EXTI1_IRQHandler() -> void { ServiceLine(1); }
extern "C" auto EXTI2_IRQHandler() -> void { ServiceLine(2); }
extern "C" auto EXTI3_IRQHandler() -> void { ServiceLine(3); }
extern "C" auto EXTI4_IRQHandler() -> void { ServiceLine(4); }
extern "C" auto EXTI9_5_IRQHandler() -> void { ServiceRange(5, 9); }
extern "C" auto EXTI15_10_IRQHandler() -> void { ServiceRange(10, 15); }

}  // namespace mcu
