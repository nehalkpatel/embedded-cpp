#pragma once

namespace mcu {

/// @brief Bring the core to a state where compiled C++ can run correctly.
///
/// Called from Reset_Handler (as SystemInit) before .data is usable and before
/// static constructors run, so it must touch nothing but core registers.
///
/// Enables the FPU and points the vector table at flash. The FPU matters even
/// for code with no floating point in sight: the toolchain compiles with
/// -mfloat-abi=hard, so the first FP instruction from anywhere -- a library
/// routine included -- traps as a UsageFault at a program counter that looks
/// entirely unrelated to the cause.
extern "C" auto SystemInit() -> void;

}  // namespace mcu
