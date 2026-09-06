#pragma once

namespace mcu {

/// @brief Bring the core to a state where compiled C++ can run correctly.
///
/// Called from Reset_Handler (as SystemInit) after .data is copied and .bss
/// zeroed, but before static constructors run. It touches nothing but core
/// registers: peripheral clocks are each driver's own business, and this
/// project never leaves the reset clock configuration (HSI, 16 MHz) at all.
///
/// Enables the FPU and points the vector table at flash. The FPU matters even
/// for code with no floating point in sight: the toolchain compiles with
/// -mfloat-abi=hard, so the first FP instruction from anywhere -- a library
/// routine included -- traps as a UsageFault at a program counter that looks
/// entirely unrelated to the cause.
extern "C" auto SystemInit() -> void;

}  // namespace mcu
