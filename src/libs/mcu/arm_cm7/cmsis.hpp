#pragma once

/// @file
/// The one place that includes ST's CMSIS device header.
///
/// The vendor header defines every peripheral register on the part. It is
/// included through an INTERFACE target marked SYSTEM (see the cmsis_f767
/// target in the top-level CMakeLists.txt) because it uses anonymous structs
/// and unions that this project's -Wpedantic -Werror would otherwise reject.
/// Routing every use through this header keeps that arrangement in one place,
/// and makes the dependency on vendor code greppable.

// NOLINTBEGIN(misc-include-cleaner)
#include <stm32f767xx.h>
// NOLINTEND(misc-include-cleaner)
