#include "libs/mcu/arm_cm7/gpio_port.hpp"

#include <cstdint>

#include "libs/mcu/arm_cm7/cmsis.hpp"
#include "libs/mcu/arm_cm7/gpio_registers.hpp"

namespace mcu {

auto PortRegisters(GpioPort port) -> GPIO_TypeDef* {
  // The port register blocks are contiguous at 0x400 intervals from GPIOA,
  // in the same order as the enumerators.
  const auto base = GPIOA_BASE + (static_cast<std::uintptr_t>(port) * 0x400UL);
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  return reinterpret_cast<GPIO_TypeDef*>(base);
}

namespace {

/// Write a `width`-bit field for `pin` into a register whose fields are packed
/// one per pin. MODER, OSPEEDR and PUPDR are two bits per pin; AFR is four,
/// split across two words.
auto WriteField(volatile std::uint32_t& reg, std::uint32_t shift,
                std::uint32_t width, std::uint32_t value) -> void {
  const std::uint32_t mask = ((1UL << width) - 1UL) << shift;
  reg = (reg & ~mask) | ((value << shift) & mask);
}

}  // namespace

auto ConfigurePin(GpioPort port, std::uint32_t pin,
                  const PinConfig& config) -> void {
  EnablePortClock(port);
  auto* registers = PortRegisters(port);

  const std::uint32_t two_bit_shift = pin * 2UL;

  WriteField(registers->OSPEEDR, two_bit_shift, 2,
             static_cast<std::uint32_t>(config.speed));
  WriteField(registers->PUPDR, two_bit_shift, 2,
             static_cast<std::uint32_t>(config.pull));

  if (config.output_type == OutputType::kOpenDrain) {
    registers->OTYPER |= 1UL << pin;
  } else {
    registers->OTYPER &= ~(1UL << pin);
  }

  if (config.mode == PinMode::kAlternate) {
    // AFR[0] covers pins 0-7, AFR[1] pins 8-15, four bits each. Must be set
    // before MODER selects alternate mode, or the pin briefly drives through
    // whichever peripheral AF0 happens to be.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    WriteField(registers->AFR[pin / 8UL], (pin % 8UL) * 4UL, 4,
               config.alternate_function);
  }

  // MODER last: it is what actually hands the pin over.
  WriteField(registers->MODER, two_bit_shift, 2,
             static_cast<std::uint32_t>(config.mode));
}

auto EnablePortClock(GpioPort port) -> void {
  RCC->AHB1ENR |= 1UL << static_cast<std::uint32_t>(port);

  // A peripheral clock enable takes a couple of cycles to take effect. Reading
  // the register back stalls until the write has landed; without this, an
  // immediately following register write can be dropped.
  static_cast<void>(RCC->AHB1ENR);
}

}  // namespace mcu
