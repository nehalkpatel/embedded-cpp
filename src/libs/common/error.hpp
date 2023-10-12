#pragma once

#include <cstdint>

enum class Error : uint32_t {
  kOk = 0,
  kUnknown,
  kInvalidArgument,
  kInvalidState,
  kInvalidOperation,
};