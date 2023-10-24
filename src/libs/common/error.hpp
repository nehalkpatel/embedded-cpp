#pragma once

#include <cstdint>

namespace common {

enum class Error : uint32_t {
  kOk = 1,
  kUnknown,
  kInvalidArgument,
  kInvalidState,
  kInvalidOperation,
};

}  // namespace common
