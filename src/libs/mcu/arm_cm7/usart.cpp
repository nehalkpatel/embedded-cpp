#include "libs/mcu/arm_cm7/usart.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <utility>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/arm_cm7/systick.hpp"
#include "libs/mcu/uart.hpp"

namespace mcu {

namespace {

// APB1 and APB2 both run at the core clock in the reset configuration (HSI
// 16 MHz, all prescalers at 1). USART1 and USART6 are on APB2, the rest on
// APB1; the distinction only starts mattering once the PLL and the bus
// prescalers are configured.
constexpr std::uint32_t kPeripheralClockHz = 16'000'000;

constexpr std::size_t kUsartCount = 4;

[[nodiscard]] auto Index(UsartId id) -> std::size_t {
  return static_cast<std::size_t>(id);
}

[[nodiscard]] auto Registers(UsartId id) -> USART_TypeDef* {
  switch (id) {
    case UsartId::kUsart1:
      return USART1;
    case UsartId::kUsart2:
      return USART2;
    case UsartId::kUsart3:
      return USART3;
    case UsartId::kUsart6:
      return USART6;
  }
  return USART3;
}

[[nodiscard]] auto IrqNumber(UsartId id) -> IRQn_Type {
  switch (id) {
    case UsartId::kUsart1:
      return USART1_IRQn;
    case UsartId::kUsart2:
      return USART2_IRQn;
    case UsartId::kUsart3:
      return USART3_IRQn;
    case UsartId::kUsart6:
      return USART6_IRQn;
  }
  return USART3_IRQn;
}

auto EnablePeripheralClock(UsartId id) -> void {
  switch (id) {
    case UsartId::kUsart1:
      RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
      static_cast<void>(RCC->APB2ENR);
      break;
    case UsartId::kUsart6:
      RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
      static_cast<void>(RCC->APB2ENR);
      break;
    case UsartId::kUsart2:
      RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
      static_cast<void>(RCC->APB1ENR);
      break;
    case UsartId::kUsart3:
      RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
      static_cast<void>(RCC->APB1ENR);
      break;
  }
}

/// Word length is M1:M0, and it counts the parity bit. Enabling parity on an
/// 8-bit format therefore needs a 9-bit word, or the parity bit would eat the
/// top data bit -- the single easiest thing to get wrong here.
[[nodiscard]] auto WordLengthBits(const UartConfig& config)
    -> std::expected<std::uint32_t, common::Error> {
  auto data_bits = 0U;
  switch (config.data_bits) {
    case UartConfig::DataBits::k7Bits:
      data_bits = 7;
      break;
    case UartConfig::DataBits::k8Bits:
      data_bits = 8;
      break;
    case UartConfig::DataBits::k9Bits:
      data_bits = 9;
      break;
  }
  const auto total =
      data_bits + (config.parity == UartConfig::Parity::kNone ? 0U : 1U);

  switch (total) {
    case 7:
      return USART_CR1_M1;  // M1:M0 = 10
    case 8:
      return 0U;  // M1:M0 = 00
    case 9:
      return USART_CR1_M0;  // M1:M0 = 01
    default:
      // 9 data bits plus parity is 10, which the peripheral cannot express.
      return std::unexpected(common::Error::kInvalidArgument);
  }
}

// The USART newlib's _write talks to, and a flag for whether it is usable.
// Set by Init; read from _write, which cannot be handed a C++ object.
USART_TypeDef* g_console = nullptr;

struct RxSlot {
  std::function<void(const std::byte*, std::size_t)> handler;
};

// One per instance, namespace scope so an interrupt can reach it before any
// function-local static would have been initialized.
std::array<RxSlot, kUsartCount> g_rx_slots;

auto ServiceRx(UsartId id) -> void {
  auto* registers = Registers(id);

  // Overrun sets RXNE's companion flag and, left alone, wedges the receiver.
  // Clearing it discards the byte that was lost, which has already happened.
  if ((registers->ISR & USART_ISR_ORE) != 0U) {
    registers->ICR = USART_ICR_ORECF;
  }

  while ((registers->ISR & USART_ISR_RXNE) != 0U) {
    // Reading RDR is what clears RXNE.
    const auto value = static_cast<std::byte>(registers->RDR & 0xFFU);
    const auto& slot = g_rx_slots.at(Index(id));
    if (slot.handler) {
      slot.handler(&value, 1);
    }
  }
}

}  // namespace

auto Usart::Init(const UartConfig& config)
    -> std::expected<void, common::Error> {
  if (initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (config.baud_rate == 0) {
    return std::unexpected(common::Error::kInvalidArgument);
  }
  // XON/XOFF is a software protocol; the peripheral has no bit for it, and
  // silently ignoring the request would be worse than refusing it.
  if (config.flow_control == UartConfig::FlowControl::kXonXoff) {
    return std::unexpected(common::Error::kInvalidArgument);
  }

  const auto word_length = WordLengthBits(config);
  if (!word_length) {
    return std::unexpected(word_length.error());
  }

  EnablePeripheralClock(id_);

  const PinConfig pin{
      .mode = PinMode::kAlternate,
      .output_type = OutputType::kPushPull,
      .speed = PinSpeed::kVeryHigh,
      .pull = PinPull::kUp,
      .alternate_function = pins_.alternate_function,
  };
  ConfigurePin(pins_.tx_port, pins_.tx_pin, pin);
  ConfigurePin(pins_.rx_port, pins_.rx_pin, pin);

  auto* registers = Registers(id_);

  // Every control bit but UE must be written while the USART is disabled.
  registers->CR1 = 0;

  registers->BRR =
      (kPeripheralClockHz + (config.baud_rate / 2U)) / config.baud_rate;

  registers->CR2 =
      config.stop_bits == UartConfig::StopBits::k2Bits ? USART_CR2_STOP_1 : 0U;

  registers->CR3 = config.flow_control == UartConfig::FlowControl::kRtsCts
                       ? (USART_CR3_RTSE | USART_CR3_CTSE)
                       : 0U;

  std::uint32_t cr1 = *word_length | USART_CR1_TE | USART_CR1_RE;
  if (config.parity != UartConfig::Parity::kNone) {
    cr1 |= USART_CR1_PCE;
    if (config.parity == UartConfig::Parity::kOdd) {
      cr1 |= USART_CR1_PS;
    }
  }
  registers->CR1 = cr1 | USART_CR1_UE;

  initialized_ = true;
  g_console = registers;
  return {};
}

auto Usart::Send(std::span<const std::byte> data)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  auto* registers = Registers(id_);

  // Send must be atomic with respect to its own receive handler. A handler
  // that echoes -- which is exactly what uart_echo does -- calls Send from
  // interrupt context, and if that preempts a Send already in progress the two
  // byte streams interleave on the wire. Worse, the outer Send's next TDR
  // write clears TC, so the inner one's completion wait can outlast the byte
  // it was waiting for.
  //
  // Masking just this USART's interrupt, rather than all of them, keeps the
  // window narrow: other peripherals keep interrupting, and a byte arriving
  // meanwhile still sets RXNE, so it is delivered as soon as the flag is
  // restored rather than lost. Called from the handler itself this is a no-op,
  // which is correct -- an interrupt cannot preempt itself.
  const bool rx_interrupt_was_enabled =
      (registers->CR1 & USART_CR1_RXNEIE) != 0U;
  registers->CR1 &= ~USART_CR1_RXNEIE;

  for (const auto value : data) {
    while ((registers->ISR & USART_ISR_TXE) == 0U) {
    }
    registers->TDR = std::to_integer<std::uint32_t>(value);
  }

  // Wait for the last byte to leave the shift register, not just the holding
  // register. Without this, returning from Send and immediately resetting or
  // reconfiguring the peripheral truncates the final character.
  while ((registers->ISR & USART_ISR_TC) == 0U) {
  }

  if (rx_interrupt_was_enabled) {
    registers->CR1 |= USART_CR1_RXNEIE;
  }
  return {};
}

auto Usart::Receive(std::span<std::byte> buffer, std::uint32_t timeout_ms)
    -> std::expected<std::size_t, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (buffer.empty()) {
    return 0U;
  }

  auto* registers = Registers(id_);
  const std::uint32_t start = Millis();
  std::size_t received = 0;

  while (received < buffer.size()) {
    if ((registers->ISR & USART_ISR_ORE) != 0U) {
      registers->ICR = USART_ICR_ORECF;
    }

    if ((registers->ISR & USART_ISR_RXNE) != 0U) {
      buffer[received] = static_cast<std::byte>(registers->RDR & 0xFFU);
      ++received;
      continue;
    }

    // timeout_ms == 0 means wait forever, per the interface contract.
    if (timeout_ms != 0 && (Millis() - start) > timeout_ms) {
      return received > 0 ? std::expected<std::size_t, common::Error>{received}
                          : std::unexpected(common::Error::kTimeout);
    }
  }

  return received;
}

auto Usart::SetRxHandler(std::function<void(const std::byte*, std::size_t)>
                             handler) -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  auto* registers = Registers(id_);
  g_rx_slots.at(Index(id_)).handler = std::move(handler);

  registers->CR1 |= USART_CR1_RXNEIE;
  NVIC_SetPriority(IrqNumber(id_), 5);
  NVIC_EnableIRQ(IrqNumber(id_));
  return {};
}

auto PutcharToConsole(char value) -> bool {
  if (g_console == nullptr) {
    return false;
  }
  while ((g_console->ISR & USART_ISR_TXE) == 0U) {
  }
  g_console->TDR =
      static_cast<std::uint32_t>(static_cast<unsigned char>(value));
  return true;
}

extern "C" auto USART1_IRQHandler() -> void { ServiceRx(UsartId::kUsart1); }
extern "C" auto USART2_IRQHandler() -> void { ServiceRx(UsartId::kUsart2); }
extern "C" auto USART3_IRQHandler() -> void { ServiceRx(UsartId::kUsart3); }
extern "C" auto USART6_IRQHandler() -> void { ServiceRx(UsartId::kUsart6); }

}  // namespace mcu
