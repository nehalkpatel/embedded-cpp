# std::expected Monadic Operators Analysis

## Executive Summary

This analysis evaluates the current use of `std::expected` in the codebase and identifies opportunities to improve code readability and compactness using C++23 monadic operators (`and_then()`, `transform()`, `or_else()`, `transform_error()`).

## Monadic Operators Overview

C++23 `std::expected` provides several monadic operators:

- **`and_then()`**: Chains operations that return `std::expected` (like `>>=` in Haskell)
- **`transform()`**: Transforms the contained value (like `fmap`)
- **`or_else()`**: Provides alternative on error (error recovery)
- **`transform_error()`**: Transforms the error value

## Current Patterns Found

### Pattern 1: Sequential Operations with Manual Error Propagation

**Location**: `src/libs/mcu/host/host_uart.cpp:50-65`

**Current Code**:
```cpp
auto HostUart::Send(std::span<const uint8_t> data)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  const UartEmulatorRequest request{/* ... */};

  auto send_result = transport_.Send(Encode(request));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  auto response_str = transport_.Receive();
  if (!response_str) {
    return std::unexpected(response_str.error());
  }

  const auto response = Decode<UartEmulatorResponse>(response_str.value());
  if (response.status != common::Error::kOk) {
    return std::unexpected(response.status);
  }

  return {};
}
```

**With Monadic Operators**:
```cpp
auto HostUart::Send(std::span<const uint8_t> data)
    -> std::expected<void, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  const UartEmulatorRequest request{/* ... */};

  return transport_.Send(Encode(request))
      .and_then([this](auto&&) { return transport_.Receive(); })
      .and_then([](const std::string& response_str)
          -> std::expected<void, common::Error> {
        const auto response = Decode<UartEmulatorResponse>(response_str);
        if (response.status != common::Error::kOk) {
          return std::unexpected(response.status);
        }
        return {};
      });
}
```

**Benefits**:
- Eliminates intermediate variables (`send_result`, `response_str`)
- Clear pipeline of operations
- Automatic error propagation
- More functional style

**Line Savings**: ~7 lines, much clearer intent

---

### Pattern 2: Receive with Transformation and Status Check

**Location**: `src/libs/mcu/host/host_uart.cpp:68-108`

**Current Code**:
```cpp
auto HostUart::Receive(std::span<uint8_t> buffer, uint32_t timeout_ms)
    -> std::expected<size_t, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  const UartEmulatorRequest request{/* ... */};

  auto send_result = transport_.Send(Encode(request));
  if (!send_result) {
    return std::unexpected(send_result.error());
  }

  auto response_str = transport_.Receive();
  if (!response_str) {
    return std::unexpected(response_str.error());
  }

  const auto response = Decode<UartEmulatorResponse>(response_str.value());
  if (response.status != common::Error::kOk) {
    return std::unexpected(response.status);
  }

  // Copy received data to buffer
  const size_t bytes_to_copy{std::min(buffer.size(), response.data.size())};
  std::copy_n(response.data.begin(), bytes_to_copy, buffer.begin());

  return bytes_to_copy;
}
```

**With Monadic Operators**:
```cpp
auto HostUart::Receive(std::span<uint8_t> buffer, uint32_t timeout_ms)
    -> std::expected<size_t, common::Error> {
  if (!initialized_) {
    return std::unexpected(common::Error::kInvalidState);
  }

  if (busy_) {
    return std::unexpected(common::Error::kInvalidOperation);
  }

  const UartEmulatorRequest request{/* ... */};

  return transport_.Send(Encode(request))
      .and_then([this](auto&&) { return transport_.Receive(); })
      .transform([](const std::string& response_str) {
        return Decode<UartEmulatorResponse>(response_str);
      })
      .and_then([buffer](const UartEmulatorResponse& response)
          -> std::expected<size_t, common::Error> {
        if (response.status != common::Error::kOk) {
          return std::unexpected(response.status);
        }

        const size_t bytes_to_copy{std::min(buffer.size(), response.data.size())};
        std::copy_n(response.data.begin(), bytes_to_copy, buffer.begin());
        return bytes_to_copy;
      });
}
```

**Benefits**:
- Clear transformation pipeline: Send → Receive → Decode → Process
- Eliminates intermediate variables
- Better expresses the data flow

---

### Pattern 3: Pin State Operations with Chaining

**Location**: `src/libs/mcu/host/host_pin.cpp:43-65`

**Current Code**:
```cpp
auto HostPin::SendState(PinState state) -> std::expected<void, common::Error> {
  const PinEmulatorRequest req = {
      .name = name_,
      .operation = OperationType::kSet,
      .state = state,
  };
  if (!transport_.Send(Encode(req))) {
    return std::unexpected(common::Error::kUnknown);
  }
  auto rx_bytes = transport_.Receive();
  if (!rx_bytes) {
    return std::unexpected(common::Error::kUnknown);
  }

  const auto resp = Decode<PinEmulatorResponse>(rx_bytes.value());

  if (resp.status != common::Error::kOk) {
    return std::unexpected(resp.status);
  }

  state_ = state;
  return {};
}
```

**With Monadic Operators**:
```cpp
auto HostPin::SendState(PinState state) -> std::expected<void, common::Error> {
  const PinEmulatorRequest req = {
      .name = name_,
      .operation = OperationType::kSet,
      .state = state,
  };

  return transport_.Send(Encode(req))
      .or_else([](auto) { return std::unexpected(common::Error::kUnknown); })
      .and_then([this](auto&&) {
        return transport_.Receive()
            .or_else([](auto) { return std::unexpected(common::Error::kUnknown); });
      })
      .transform([](const std::string& rx_bytes) {
        return Decode<PinEmulatorResponse>(rx_bytes);
      })
      .and_then([this, state](const PinEmulatorResponse& resp)
          -> std::expected<void, common::Error> {
        if (resp.status != common::Error::kOk) {
          return std::unexpected(resp.status);
        }
        state_ = state;
        return {};
      });
}
```

**Benefits**:
- Single expression pipeline
- `or_else()` for error transformation
- Clear data flow

---

### Pattern 4: Board Initialization with Multiple Steps

**Location**: `src/libs/board/host/host_board.cpp:11-33`

**Current Code**:
```cpp
auto HostBoard::Init() -> std::expected<void, common::Error> {
  {
    const auto res{user_led_1_.Configure(mcu::PinDirection::kOutput)};
    if (!res) {
      return res;
    }
  }

  {
    const auto res{user_led_2_.Configure(mcu::PinDirection::kOutput)};
    if (!res) {
      return res;
    }
  }

  {
    const auto res{user_button_1_.Configure(mcu::PinDirection::kInput)};
    if (!res) {
      return res;
    }
  }
  return {};
}
```

**With Monadic Operators**:
```cpp
auto HostBoard::Init() -> std::expected<void, common::Error> {
  return user_led_1_.Configure(mcu::PinDirection::kOutput)
      .and_then([this](auto&&) {
        return user_led_2_.Configure(mcu::PinDirection::kOutput);
      })
      .and_then([this](auto&&) {
        return user_button_1_.Configure(mcu::PinDirection::kInput);
      });
}
```

**Benefits**:
- Eliminates all scoped blocks and intermediate variables
- Very clear sequential initialization
- Automatic error propagation
- Much more compact (3 lines vs 22 lines)

**Line Savings**: ~19 lines!

---

### Pattern 5: Application Init with State Checking

**Location**: `src/apps/uart_echo/uart_echo.cpp:28-59`

**Current Code**:
```cpp
auto UartEcho::Init() -> std::expected<void, common::Error> {
  // Initialize the board
  auto status = board_.Init();
  if (!status) {
    return status;
  }

  // Initialize UART
  const mcu::UartConfig uart_config{};
  auto uart_init = board_.Uart1().Init(uart_config);
  if (!uart_init) {
    return uart_init;
  }

  // Set up RxHandler to echo received data back and toggle LED
  auto handler_result =
      board_.Uart1().SetRxHandler([this](const uint8_t* data, size_t size) {
        // Echo the data back
        std::vector<uint8_t> echo_data(data, data + size);
        std::ignore = board_.Uart1().Send(echo_data);

        // Toggle LED1 to indicate data received
        auto led_state = board_.UserLed1().Get();
        if (led_state && led_state.value() == mcu::PinState::kHigh) {
          std::ignore = board_.UserLed1().SetLow();
        } else {
          std::ignore = board_.UserLed1().SetHigh();
        }
      });

  return handler_result;
}
```

**With Monadic Operators**:
```cpp
auto UartEcho::Init() -> std::expected<void, common::Error> {
  const mcu::UartConfig uart_config{};

  return board_.Init()
      .and_then([this, &uart_config](auto&&) {
        return board_.Uart1().Init(uart_config);
      })
      .and_then([this](auto&&) {
        return board_.Uart1().SetRxHandler([this](const uint8_t* data, size_t size) {
          // Echo the data back
          std::vector<uint8_t> echo_data(data, data + size);
          std::ignore = board_.Uart1().Send(echo_data);

          // Toggle LED1 to indicate data received
          auto led_state = board_.UserLed1().Get();
          if (led_state && led_state.value() == mcu::PinState::kHigh) {
            std::ignore = board_.UserLed1().SetLow();
          } else {
            std::ignore = board_.UserLed1().SetHigh();
          }
        });
      });
}
```

**Benefits**:
- Eliminates intermediate variables (`status`, `uart_init`, `handler_result`)
- Clear initialization pipeline
- More compact

---

### Pattern 6: Blinky Run Loop with State Transitions

**Location**: `src/apps/blinky/blinky.cpp:27-45`

**Current Code**:
```cpp
auto Blinky::Run() -> std::expected<void, common::Error> {
  auto status = board_.UserLed1().SetHigh();
  while (true) {
    if (!status) {
      return std::unexpected(status.error());
    }
    mcu::Delay(500ms);
    auto state = board_.UserLed1().Get();
    if (!state) {
      return std::unexpected(state.error());
    }
    if (state.value() == mcu::PinState::kHigh) {
      status = board_.UserLed1().SetLow();
    } else {
      status = board_.UserLed1().SetHigh();
    }
  }
  return {};
}
```

**With Monadic Operators**:
```cpp
auto Blinky::Run() -> std::expected<void, common::Error> {
  auto status = board_.UserLed1().SetHigh();

  while (true) {
    status = status.and_then([this](auto&&) {
      mcu::Delay(500ms);
      return board_.UserLed1().Get();
    }).and_then([this](mcu::PinState state) {
      return (state == mcu::PinState::kHigh)
          ? board_.UserLed1().SetLow()
          : board_.UserLed1().SetHigh();
    });

    if (!status) {
      return std::unexpected(status.error());
    }
  }
  return {};
}
```

**Benefits**:
- More functional approach to state transitions
- Clearer expression of: "get state, then toggle based on state"
- Single status variable that's updated each iteration

---

### Pattern 7: Blinky Init with Conditional Execution

**Location**: `src/apps/blinky/blinky.cpp:47-55`

**Current Code**:
```cpp
auto Blinky::Init() -> std::expected<void, common::Error> {
  auto status = board_.Init();
  if (status) {
    return board_.UserButton1().SetInterruptHandler(
        [this]() { std::ignore = board_.UserLed2().SetHigh(); },
        mcu::PinTransition::kRising);
  }
  return status;
}
```

**With Monadic Operators**:
```cpp
auto Blinky::Init() -> std::expected<void, common::Error> {
  return board_.Init()
      .and_then([this](auto&&) {
        return board_.UserButton1().SetInterruptHandler(
            [this]() { std::ignore = board_.UserLed2().SetHigh(); },
            mcu::PinTransition::kRising);
      });
}
```

**Benefits**:
- More concise
- Clear dependency: "init board, then set up interrupt handler"
- No intermediate variable needed

---

## Summary of Opportunities

| File | Function | Lines Saved | Readability Impact |
|------|----------|-------------|-------------------|
| host_uart.cpp | Send() | ~7 | High - clearer pipeline |
| host_uart.cpp | Receive() | ~5 | High - clear transformation |
| host_pin.cpp | SendState() | ~4 | Medium - more functional |
| host_board.cpp | Init() | ~19 | Very High - dramatic improvement |
| uart_echo.cpp | Init() | ~6 | High - clearer sequence |
| blinky.cpp | Init() | ~3 | High - very clear |
| blinky.cpp | Run() | ~3 | Medium - more functional |

**Total Estimated Line Reduction**: ~47 lines across key functions

## Recommendations

### High Priority (Significant Improvement)

1. **HostBoard::Init()** - Dramatic improvement in readability and compactness
2. **HostUart::Send() and Receive()** - Clear pipeline operations
3. **UartEcho::Init()** - Clear initialization sequence
4. **Blinky::Init()** - Simple and effective

### Medium Priority (Moderate Improvement)

5. **HostPin::SendState()** - Good use of `or_else()` for error transformation
6. **Blinky::Run()** - More functional, though the loop structure may be debatable

### Considerations

**Pros of Monadic Operators**:
- More functional, declarative style
- Automatic error propagation
- Less boilerplate
- Clearer data flow pipelines
- Fewer intermediate variables

**Cons/Tradeoffs**:
- Lambda capture can be verbose (e.g., `[this]`, `[this, &config]`)
- May be less familiar to developers new to functional programming
- Slightly harder to debug (can't inspect intermediate values as easily)
- Potential for deeply nested lambdas if not careful

**Best Practices**:
- Use for sequential operations that naturally form a pipeline
- Avoid overuse - simple if-statements are sometimes clearer
- Keep lambda bodies short and simple
- Consider extracting complex lambdas to named functions
- Document complex chains with comments

## Implementation Strategy

1. **Start with HostBoard::Init()** - Clearest win, sets the pattern
2. **Refactor UART operations** - High-value, frequently used
3. **Update application Init() functions** - Good examples for users
4. **Consider Blinky::Run()** - More controversial, evaluate team preference

## Conclusion

The codebase would benefit significantly from using monadic operators, particularly in initialization sequences and I/O pipeline operations. The most compelling cases are:

- **HostBoard::Init()**: 19 lines → 7 lines, much clearer
- **HostUart operations**: Clear send→receive→decode→process pipelines
- **Application Init functions**: Clear sequential setup

These changes would make the code more idiomatic C++23, reduce boilerplate, and improve readability without sacrificing correctness or debuggability.
