#include "libs/mcu/arm_cm7/i2c.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "libs/common/error.hpp"
#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/gpio_port.hpp"
#include "libs/mcu/arm_cm7/systick.hpp"

namespace mcu {

namespace {

/// Timing for 100 kHz standard mode from a 16 MHz I2CCLK (the reset
/// configuration: HSI, no PLL, APB1 prescaler 1). The F7's I2C is the v2
/// peripheral: timing is these five fields, not the old CCR divisor, and the
/// value is normally taken from ST's tool rather than derived. Matches
/// AN4235's table for 16 MHz; the arithmetic, so the number is checkable:
///
///   PRESC  = 3      -> t_PRESC = (3+1) x 62.5 ns = 250 ns
///   SCLL   = 0x13   -> (19+1) x 250 ns = 5.00 us   low period
///   SCLH   = 0x0F   -> (15+1) x 250 ns = 4.00 us   high period
///   SDADEL = 0x2    -> 2 x 250 ns = 500 ns         data hold
///   SCLDEL = 0x4    -> (4+1) x 250 ns = 1.25 us    data setup
///
/// 5.00 + 4.00 us plus rise and fall gives a ~10 us period: 100 kHz.
/// Raising the core clock invalidates this constant.
constexpr std::uint32_t kTiming100kHzAt16MHz = 0x3042'0F13U;

/// A transfer that makes no progress must fail rather than spin forever: a
/// bus held low by a stuck device never sets any completion flag.
constexpr std::uint32_t kTransferTimeoutMs = 25;

[[nodiscard]] auto Registers(I2CId id) -> I2C_TypeDef* {
  switch (id) {
    case I2CId::kI2C1:
      return I2C1;
    case I2CId::kI2C2:
      return I2C2;
    case I2CId::kI2C3:
      return I2C3;
    case I2CId::kI2C4:
      return I2C4;
  }
  return I2C1;
}

auto EnablePeripheralClock(I2CId id) -> void {
  switch (id) {
    case I2CId::kI2C1:
      RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
      break;
    case I2CId::kI2C2:
      RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
      break;
    case I2CId::kI2C3:
      RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;
      break;
    case I2CId::kI2C4:
      RCC->APB1ENR |= RCC_APB1ENR_I2C4EN;
      break;
  }
  static_cast<void>(RCC->APB1ENR);
}

/// Spin until `flag` appears in ISR, giving up after kTransferTimeoutMs.
/// Reports a NACK distinctly from a timeout: an unanswered address is a
/// missing device, which is worth telling apart from a wedged bus.
[[nodiscard]] auto WaitForFlag(I2C_TypeDef* registers, std::uint32_t flag)
    -> std::expected<void, common::Error> {
  const std::uint32_t start = Millis();
  while ((registers->ISR & flag) == 0U) {
    if ((registers->ISR & I2C_ISR_NACKF) != 0U) {
      registers->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
      return std::unexpected(common::Error::kOperationFailed);
    }
    if ((Millis() - start) > kTransferTimeoutMs) {
      return std::unexpected(common::Error::kTimeout);
    }
  }
  return {};
}

/// Program CR2 for one autoend transfer and issue START. The peripheral takes
/// the 7-bit address left-shifted by one, in the position the R/W bit occupies
/// on the wire.
auto StartTransfer(I2C_TypeDef* registers, std::uint16_t address,
                   std::size_t byte_count, bool reading) -> void {
  std::uint32_t cr2 =
      (static_cast<std::uint32_t>(address) << 1U) & I2C_CR2_SADD_Msk;
  cr2 |= (static_cast<std::uint32_t>(byte_count) << I2C_CR2_NBYTES_Pos) &
         I2C_CR2_NBYTES_Msk;
  cr2 |= I2C_CR2_AUTOEND;
  if (reading) {
    cr2 |= I2C_CR2_RD_WRN;
  }
  registers->CR2 = cr2 | I2C_CR2_START;
}

/// Autoend issues STOP itself; STOPF must still be cleared or the next
/// transfer starts against a stale flag.
auto FinishTransfer(I2C_TypeDef* registers)
    -> std::expected<void, common::Error> {
  auto stopped = WaitForFlag(registers, I2C_ISR_STOPF);
  registers->ICR = I2C_ICR_STOPCF;
  registers->CR2 = 0;
  return stopped;
}

}  // namespace

auto I2CBus::Init() -> std::expected<void, common::Error> {
  if (initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  EnablePeripheralClock(id_);

  // Open drain, because I2C signals are wire-AND: a push-pull driver would
  // fight any other device pulling the line low. The internal pull-ups are
  // weak (~40k) and adequate at 100 kHz for short wiring; a real bus wants
  // external resistors.
  const PinConfig pin{
      .mode = PinMode::kAlternate,
      .output_type = OutputType::kOpenDrain,
      .speed = PinSpeed::kVeryHigh,
      .pull = PinPull::kUp,
      .alternate_function = pins_.alternate_function,
  };
  ConfigurePin(pins_.scl_port, pins_.scl_pin, pin);
  ConfigurePin(pins_.sda_port, pins_.sda_pin, pin);

  auto* registers = Registers(id_);

  // TIMINGR is writable only while the peripheral is disabled.
  registers->CR1 &= ~I2C_CR1_PE;
  registers->TIMINGR = kTiming100kHzAt16MHz;
  registers->CR1 |= I2C_CR1_PE;

  initialized_ = true;
  return {};
}

auto I2CBus::SendData(std::uint16_t address, std::span<const std::byte> data)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (data.empty()) {
    return {};
  }

  auto* registers = Registers(id_);
  StartTransfer(registers, address, data.size(), /*reading=*/false);

  for (const auto value : data) {
    // TXIS, not TXE: TXIS means the peripheral is asking for the next byte,
    // which is also where an unanswered address surfaces as NACKF.
    if (auto ready = WaitForFlag(registers, I2C_ISR_TXIS); !ready) {
      static_cast<void>(FinishTransfer(registers));
      return std::unexpected(ready.error());
    }
    registers->TXDR = std::to_integer<std::uint32_t>(value);
  }

  return FinishTransfer(registers);
}

auto I2CBus::ReceiveData(std::uint16_t address, std::span<std::byte> buffer)
    -> std::expected<std::size_t, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }
  if (buffer.empty()) {
    return 0U;
  }

  auto* registers = Registers(id_);
  StartTransfer(registers, address, buffer.size(), /*reading=*/true);

  std::size_t received = 0;
  for (auto& slot : buffer) {
    if (auto ready = WaitForFlag(registers, I2C_ISR_RXNE); !ready) {
      static_cast<void>(FinishTransfer(registers));
      return std::unexpected(ready.error());
    }
    slot = static_cast<std::byte>(registers->RXDR & 0xFFU);
    ++received;
  }

  if (auto finished = FinishTransfer(registers); !finished) {
    return std::unexpected(finished.error());
  }
  return received;
}

}  // namespace mcu
