#pragma once

#include <cstdint>

namespace mcu {

/// @brief Which GPIO port a pin belongs to.
///
/// A named enum rather than the vendor's `GPIO_TypeDef*` so that this header,
/// and everything that includes it, stays free of CMSIS. That is not tidiness:
/// stm32f767xx.h defines `I2C1`, `USART3` and a few hundred other bare
/// identifiers as macros, and any header that pulls it in silently rewrites
/// matching names in the code around it -- including `board::Board::I2C1()`.
/// cmsis.hpp is included only from .cpp files for that reason.
///
/// The enumerator values are the port index, which is also the RCC_AHB1ENR bit
/// position and the value SYSCFG_EXTICR wants.
enum class GpioPort : std::uint8_t {
  kA = 0,
  kB,
  kC,
  kD,
  kE,
  kF,
  kG,
  kH,
  kI,
  kJ,
  kK,
};

/// @brief What a pin is wired to. MODER field values.
///
/// The portable mcu::PinDirection has only input and output, which is the
/// right vocabulary for an application. A peripheral driver needs more: a
/// USART or I2C pin is in alternate-function mode, driven by the peripheral
/// rather than by software. That distinction is backend-private, so it lives
/// here rather than in libs/mcu/pin.hpp.
enum class PinMode : std::uint8_t {
  kInput = 0b00,
  kOutput = 0b01,
  kAlternate = 0b10,
  kAnalog = 0b11,
};

enum class OutputType : std::uint8_t { kPushPull = 0, kOpenDrain = 1 };

enum class PinSpeed : std::uint8_t {
  kLow = 0b00,
  kMedium = 0b01,
  kHigh = 0b10,
  kVeryHigh = 0b11,
};

enum class PinPull : std::uint8_t { kNone = 0b00, kUp = 0b01, kDown = 0b10 };

/// @brief Everything MODER/OTYPER/OSPEEDR/PUPDR/AFR say about one pin.
///
/// Defaults are a plain input, which is also the chip's reset state, so a
/// caller only names what it needs to differ.
struct PinConfig {
  PinMode mode = PinMode::kInput;
  OutputType output_type = OutputType::kPushPull;
  PinSpeed speed = PinSpeed::kLow;
  PinPull pull = PinPull::kNone;
  /// Only consulted when mode is kAlternate. AF0-AF15; see the part datasheet's
  /// alternate function mapping table, not the reference manual.
  std::uint8_t alternate_function = 0;
};

/// @brief Apply a configuration to one pin, enabling its port clock first.
///
/// Writes the five registers in the order the reference manual's examples use:
/// everything else before MODER, so the pin never spends a cycle driving with
/// a stale output type or speed.
auto ConfigurePin(GpioPort port, std::uint32_t pin,
                  const PinConfig& config) -> void;

/// @brief Enable the AHB1 clock for one GPIO port, and wait for it to land.
///
/// Every GPIO register reads as zero and ignores writes until its port clock
/// is running -- silently, with no fault. This is the most common way to spend
/// an evening on bare-metal GPIO, so nothing here touches a port without
/// calling this first.
auto EnablePortClock(GpioPort port) -> void;

}  // namespace mcu
