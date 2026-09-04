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

/// @brief Enable the AHB1 clock for one GPIO port, and wait for it to land.
///
/// Every GPIO register reads as zero and ignores writes until its port clock
/// is running -- silently, with no fault. This is the most common way to spend
/// an evening on bare-metal GPIO, so nothing here touches a port without
/// calling this first.
auto EnablePortClock(GpioPort port) -> void;

}  // namespace mcu
