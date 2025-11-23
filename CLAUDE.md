# CLAUDE.md - AI Assistant Guide for Embedded C++ BSP

This document provides comprehensive guidance for AI assistants working with this embedded C++ Board Support Package (BSP) repository.

## Project Overview

**Purpose**: This is a demonstrative/educational embedded systems project exploring modern C++ (C++23) practices and software engineering principles in embedded contexts.

**Key Goals**:
- Apply modern C++ features to embedded systems
- Build a host-side simulation and testing environment
- Demonstrate correct-by-construction design patterns
- Create clean hardware abstraction layers

**Not Production**: This is a learning/demonstration project, not a production framework.

## Technology Stack

- **Language**: C++23 with strict compiler flags
- **Build System**: CMake 3.27+ with Ninja Multi-Config
- **Compilers**: Clang/LLVM (preferred for host), ARM GCC (for embedded targets)
- **Testing**: Google Test (C++), pytest (Python integration tests)
- **IPC**: ZeroMQ with JSON serialization
- **Embedded Targets**: STM32F3, STM32F7 Nucleo, nRF52832
- **MCU Architectures**: ARM Cortex-M4, ARM Cortex-M7

## Repository Structure

```
embedded-cpp/
├── .devcontainer/             # VS Code DevContainer configuration
│   ├── devcontainer.json      # DevContainer settings and extensions
│   └── docker-compose.devcontainer.yml  # DevContainer-specific compose overrides
│
├── .github/                   # GitHub configuration
│   └── workflows/             # GitHub Actions CI/CD
│       └── ci.yml             # Main CI workflow (build + test on main/PRs)
│
├── cmake/                     # CMake configuration and toolchains
│   └── toolchain/             # Cross-compilation toolchains
│       ├── host-clang.cmake   # Host builds (testing/dev)
│       ├── armgcc.cmake       # ARM GCC base configuration
│       ├── armgcc-cm4.cmake   # Cortex-M4 builds
│       └── armgcc-cm7.cmake   # Cortex-M7 builds
│
├── src/                       # Main source code
│   ├── apps/                  # Application layer
│   │   └── blinky/           # Example LED blink application
│   │
│   └── libs/                  # Library abstractions
│       ├── common/           # Common utilities (error handling)
│       │   └── error.hpp     # std::expected-based error handling
│       │
│       ├── mcu/              # MCU abstraction layer (HAL)
│       │   ├── pin.hpp       # Pin interface (Input/Output/Bidirectional)
│       │   ├── i2c.hpp       # I2C controller interface
│       │   ├── delay.hpp     # Delay/timing interface
│       │   │
│       │   └── host/         # Host emulator implementations
│       │       ├── host_pin.hpp/cpp        # ZMQ-based pin emulation
│       │       ├── zmq_transport.hpp/cpp   # ZeroMQ IPC transport
│       │       ├── dispatcher.hpp          # Message routing
│       │       └── *_messages.hpp          # Message definitions
│       │
│       └── board/            # Board abstraction layer (BSP)
│           ├── board.hpp     # Board interface
│           ├── host/         # Host board implementation
│           ├── stm32f3_discovery/  # STM32F3 Discovery
│           ├── stm32f767zi_nucleo/ # STM32F7 Nucleo
│           └── nrf52832_dk/  # Nordic nRF52 DK
│
├── py/                        # Python components
│   └── host-emulator/        # Python-based hardware emulator
│       ├── src/              # Emulator implementation
│       │   └── emulator.py   # Virtual device simulator
│       └── tests/            # Integration tests
│           └── test_blinky.py
│
├── test/                      # System-level tests
├── external/                  # External dependencies (placeholder)
│
├── CMakeLists.txt            # Root build configuration
├── CMakePresets.json         # Build presets (host, arm-cm4, arm-cm7, etc.)
├── Dockerfile                # Docker development environment
├── docker-compose.yml        # Docker Compose configuration
├── README.md                 # Project overview
└── CLAUDE.md                 # This file
```

## Architecture

### Layered Design

The codebase follows a strict layered architecture with dependency inversion:

```
┌─────────────────────────────────────┐
│   Application Layer (apps/)         │  ← User applications (blinky)
└─────────────────────────────────────┘
              ↓ depends on
┌─────────────────────────────────────┐
│   Board Abstraction (libs/board/)   │  ← Board interface (LEDs, buttons, I2C)
└─────────────────────────────────────┘
              ↓ depends on
┌─────────────────────────────────────┐
│   MCU Abstraction (libs/mcu/)       │  ← Hardware interfaces (Pin, I2C, Delay)
└─────────────────────────────────────┘
              ↓ depends on
┌─────────────────────────────────────┐
│   Platform Implementations          │  ← Host/STM32/nRF52 specific code
└─────────────────────────────────────┘
```

**Key Principle**: Upper layers depend on abstract interfaces, not concrete implementations. Platform selection happens at CMake configuration time via presets.

### Core Abstractions

#### 1. Error Handling (`libs/common/error.hpp`)

```cpp
enum class Error {
  kOk,
  kUnknown,
  kInvalidArgument,
  kInvalidState,
  kInvalidOperation,
  // ... more error codes
};
```

**Pattern**: All operations that can fail return `std::expected<T, Error>` instead of throwing exceptions (no exceptions in embedded).

**Usage**:
```cpp
auto result = SomeOperation();
if (!result) {
  return std::unexpected(result.error());
}
// Use result.value()
```

#### 2. Pin Abstraction (`libs/mcu/pin.hpp`)

```cpp
enum class PinDirection { kInput, kOutput };
enum class PinState { kLow, kHigh, kHighZ };

struct InputPin {
  virtual auto Get() -> PinState = 0;
  virtual auto SetInterruptHandler(InterruptCallback) -> void = 0;
};

struct OutputPin {
  virtual auto SetHigh() -> void = 0;
  virtual auto SetLow() -> void = 0;
};

struct BidirectionalPin : InputPin, OutputPin {
  virtual auto Configure(PinDirection) -> std::expected<void, Error> = 0;
};
```

#### 3. Board Interface (`libs/board/board.hpp`)

```cpp
struct Board {
  virtual auto Init() -> std::expected<void, Error> = 0;
  virtual auto UserLed1() -> mcu::OutputPin& = 0;
  virtual auto UserLed2() -> mcu::OutputPin& = 0;
  virtual auto UserButton1() -> mcu::InputPin& = 0;
  virtual auto I2C1() -> mcu::I2CController& = 0;
};
```

### Host Emulation Architecture

The "host" platform is special - it enables desktop development and testing:

```
┌──────────────────┐         ZMQ/JSON         ┌──────────────────┐
│  C++ Application │ ←──────────────────────→ │ Python Emulator  │
│   (host build)   │     IPC over sockets     │  (virtual HW)    │
└──────────────────┘                          └──────────────────┘
```

**Components**:
- **Transport Layer** (`zmq_transport.hpp`): ZeroMQ PAIR socket communication
- **Message Protocol** (`host_emulator_messages.hpp`): Request/response messages
- **Dispatcher** (`dispatcher.hpp`): Routes messages to receivers using predicates
- **JSON Encoding** (`emulator_message_json_encoder.hpp`): Serialization
- **Python Emulator** (`py/host-emulator/src/emulator.py`): Virtual hardware simulator

**Flow**:
1. C++ code calls `pin.SetHigh()`
2. HostPin serializes request to JSON
3. ZMQ transport sends to Python emulator
4. Emulator updates virtual pin state
5. Response sent back to C++
6. Integration tests verify behavior

## Development Environment

### Docker and DevContainers

The project provides a complete Docker-based development environment with VS Code DevContainer support.

**Dockerfile** (`Dockerfile`):
- Base image: mcr.microsoft.com/devcontainers/base:ubuntu24.04
- Pre-installed tools:
  - **Build**: cmake, ninja-build, build-essential
  - **Clang/LLVM**: clang-18, clang-format, clang-tidy, lld, lldb, libc++-dev
  - **Python**: python3, pip, venv
  - **ARM Toolchain**: gcc-arm-none-eabi, gdb, gdb-multiarch
  - **Libraries**: libzmq3-dev
  - **Optional dev tools** (when `INSTALL_DEV_TOOLS=true`): bat, fzf, htop, nano, ripgrep, tree

**Docker Compose** (`docker-compose.yml`):
- **embedded-cpp-dev**: Main development service
  - Mounts workspace at `/home/vscode/workspace`
  - Named volume `build-cache` for faster incremental builds
  - Image tag: `embedded-cpp-docker:latest`
- **host-debug**: Runs host-debug workflow preset
- **host-release**: Runs host-release workflow preset

**DevContainer** (`.devcontainer/`):
- **devcontainer.json**: VS Code configuration
  - Pre-configured extensions: C++ tools, CMake, Cortex-Debug, Python, Ruff, GitLens, Error Lens
  - CMake settings: source dir, build dir, Ninja generator
  - Remote user: `vscode`
  - Workspace folder: `/home/vscode/workspace`
- **docker-compose.devcontainer.yml**: DevContainer-specific overrides
  - Sets `INSTALL_DEV_TOOLS=true` for additional CLI tools
  - Uses separate image tag: `embedded-cpp-docker:devcontainer`

**Usage**:
```bash
# Open in VS Code DevContainer
code .
# Then: Ctrl+Shift+P → "Dev Containers: Reopen in Container"

# Or use docker-compose directly
docker compose up embedded-cpp-dev
docker compose run --rm host-debug
docker compose run --rm host-release
```

### Continuous Integration

**GitHub Actions** (`.github/workflows/ci.yml`):
- **Triggers**: Push to main, pull requests to main, manual workflow dispatch
- **Job**: `host-build-test`
  - Runs on: ubuntu-latest
  - Steps:
    1. Checkout repository
    2. Build Docker image via docker-compose
    3. Run host-debug workflow (configure + build + test)
    4. Upload test results (XML/logs from `build/host/Testing/`)
- All tests must pass before merging to main

## Build System

### CMake Presets

Build configurations are defined in `CMakePresets.json`:

| Preset | MCU | Board | Toolchain | Use Case | Status |
|--------|-----|-------|-----------|----------|--------|
| `host` | host | host | Clang | Development, testing, debugging | ✅ Fully working |
| `arm-cm4` | arm_cm4 | - | ARM GCC | Base Cortex-M4 builds | ⚙️ Base preset (hidden) |
| `arm-cm7` | arm_cm7 | - | ARM GCC | Base Cortex-M7 builds | ⚙️ Base preset (hidden) |
| `stm32f3_discovery` | arm_cm4 | stm32f3_discovery | ARM GCC | STM32F3 Discovery board | 🚧 Partial (files present, no C++ impl) |

### Build Commands

```bash
# Using CMake Workflow Presets (Recommended)
cmake --workflow --preset=host-debug          # Configure + build + test (Debug)
cmake --workflow --preset=host-release        # Configure + build + test (Release)
cmake --workflow --preset=stm32f3_discovery-release  # Configure + build (no tests on hardware)

# Manual CMake Commands
# Configure for host (development/testing)
cmake --preset=host

# Build host configuration
cmake --build --preset=host --config Debug

# Run tests
ctest --preset=host -C Debug

# Configure for STM32F3 Discovery
cmake --preset=stm32f3_discovery

# Build for hardware
cmake --build --preset=stm32f3_discovery --config Release

# Using Docker Compose
docker compose run --rm host-debug        # Run host-debug workflow in container
docker compose run --rm host-release      # Run host-release workflow in container
```

### External Dependencies

Managed via CMake `FetchContent`:

| Library | Version | Purpose | When Fetched |
|---------|---------|---------|--------------|
| etl | 20.38.1 | Embedded Template Library (STL alternative) | Always |
| googletest | v1.14.0 | C++ unit testing | Host builds only |
| cppzmq | v4.10.0 | C++ bindings for ZeroMQ | Host builds only |
| nlohmann/json | v3.11.2 | JSON serialization | Host builds only |
| STM32CubeF7 | v1.17.1 | STM32F7 HAL library | arm_cm7 builds only |
| cmake-scripts | main | CMake build utilities | Always |

**Note**: Dependencies are fetched automatically during CMake configuration based on the selected preset.

## Code Style and Conventions

### Formatting (`.clang-format`)

- **Base Style**: Google style guide
- **Pointer Alignment**: Left (`int* ptr`, not `int *ptr`)
- **Newline at EOF**: Enforced
- **Indentation**: 2 spaces

### Linting (`.clang-tidy`)

**Enabled Check Categories**:
- `bugprone-*`: Bug-prone code patterns
- `google-*`: Google style guide
- `misc-*`: Miscellaneous checks
- `modernize-*`: Modern C++ features
- `performance-*`: Performance issues
- `portability-*`: Portability concerns
- `readability-*`: Code readability

**Warnings as Errors**: All enabled checks produce errors, not warnings.

### Naming Conventions

Enforced by `clang-tidy`:

| Element | Convention | Example |
|---------|-----------|---------|
| Namespaces | `snake_case` | `common`, `mcu`, `board` |
| Classes/Structs | `PascalCase` | `InputPin`, `HostBoard` |
| Functions/Methods | `PascalCase` | `SetHigh()`, `Configure()` |
| Variables | `snake_case` | `direction`, `pin_state` |
| Private Members | `snake_case_` | `transport_`, `state_` |
| Constants | `kPascalCase` | `kOk`, `kInvalidState` |
| Macros | `UPPER_CASE` | `GPIO_PIN_SET` |
| Enum Values | `kPascalCase` | `kInput`, `kOutput` |

### Include Order

```cpp
// 1. System/STL headers
#include <chrono>
#include <expected>
#include <memory>

// 2. Third-party libraries
#include "etl/span.h"
#include "zmq.hpp"

// 3. Project headers (relative to src/)
#include "libs/common/error.hpp"
#include "libs/mcu/pin.hpp"
```

### Code Organization

- **Header-only when possible**: Enables optimization and reduces compilation units
- **Interface separation**: Abstract interfaces (`.hpp`) separate from implementations (`.cpp`)
- **No circular dependencies**: Strict layering prevents cycles
- **Minimal includes**: Forward declarations preferred over includes when possible

## Testing Infrastructure

### Unit Tests (C++)

**Framework**: Google Test

**Location**: Colocated with implementation (`src/libs/mcu/host/test_*.cpp`)

**Example**:
```cpp
#include <gtest/gtest.h>
#include "libs/mcu/host/zmq_transport.hpp"

TEST(ZmqTransportTest, SendReceive) {
  // Test implementation
}
```

**Running**:
```bash
ctest --preset=host -C Debug -R test_zmq_transport
```

### Integration Tests (Python)

**Framework**: pytest

**Location**: `py/host-emulator/tests/`

**Key Files**:
- `conftest.py`: Fixtures for emulator and blinky executable
- `test_blinky.py`: End-to-end behavior tests

**Fixtures**:
- `emulator`: Starts Python emulator subprocess
- `blinky`: Starts C++ blinky application subprocess
- Both fixtures manage lifecycle (startup, teardown, cleanup)

**Running**:
```bash
# From py/host-emulator/
pytest tests/ --blinky=/path/to/blinky

# Via CMake/CTest
ctest --preset=host -C Debug -R pytest
```

**Test Strategy**:
1. Start Python emulator (listens on ZMQ socket)
2. Start C++ application (connects to emulator)
3. Emulator intercepts pin operations and verifies behavior
4. Test asserts expected state changes (LED blinks, button presses)

## Development Workflows

### Adding a New Feature

1. **Identify Layer**: Determine if feature belongs in MCU, Board, or App layer
2. **Define Interface**: Add abstract interface in appropriate header
3. **Implement for Host**: Create host implementation first for testing
4. **Write Tests**: Add unit tests (C++) and/or integration tests (Python)
5. **Run Linting**: Ensure `clang-tidy` passes
6. **Format Code**: Run `clang-format` on modified files
7. **Build and Test**: Verify all tests pass
8. **Implement Hardware**: Add hardware-specific implementations as needed

### Adding a New Board

1. **Create Board Directory**: `src/libs/board/<board_name>/`
2. **Implement Board Interface**: Subclass `Board` from `board.hpp`
3. **Add CMakeLists.txt**: Define board library and dependencies
4. **Create Preset**: Add configuration in `CMakePresets.json`
5. **Update Root CMake**: Ensure preset conditionally includes board directory
6. **Document**: Add board-specific notes to this file

### Debugging Host Applications

**Advantages of Host Build**:
- Use familiar debuggers (lldb, gdb)
- Faster iteration (no flashing)
- Integration with system tools
- Python emulator inspection
- Works in DevContainer with VS Code debugging

**Workflow (DevContainer)**:
```bash
# In VS Code DevContainer terminal
cmake --workflow --preset=host-debug

# Use VS Code debugger (F5) with launch.json configuration

# Or run under debugger manually
lldb build/host/bin/blinky
```

**Workflow (Local/Docker)**:
```bash
# Build with debug symbols
cmake --build --preset=host --config Debug

# Run under debugger
lldb build/host/bin/blinky

# Or run with Python emulator manually
cd py/host-emulator
python -m src.emulator &  # Start emulator
../../build/host/bin/blinky  # Run application
```

### Adding Tests

**Unit Test**:
```cpp
// In appropriate test file (e.g., test_new_feature.cpp)
#include <gtest/gtest.h>

TEST(NewFeatureTest, BasicBehavior) {
  // Setup
  // Execute
  // Verify
}
```

**Integration Test**:
```python
# In py/host-emulator/tests/test_new_feature.py
def test_new_feature(emulator, blinky):
    # Given
    # When
    # Then
```

**Run Tests**:
```bash
ctest --preset=host -C Debug --output-on-failure
```

## Common Patterns

### Error Propagation

```cpp
auto DoSomething() -> std::expected<Result, common::Error> {
  auto step1 = Step1();
  if (!step1) {
    return std::unexpected(step1.error());
  }

  auto step2 = Step2(step1.value());
  if (!step2) {
    return std::unexpected(step2.error());
  }

  return Result{step2.value()};
}
```

### Pin Interrupt Handling

```cpp
auto SetupButton() -> void {
  button_.SetInterruptHandler([this](auto transition) {
    if (transition == mcu::PinStateTransition::kRisingEdge) {
      led_.SetHigh();
    }
  });
}
```

### Dependency Injection

```cpp
class Blinky {
 public:
  Blinky(mcu::OutputPin& led, mcu::InputPin& button)
    : led_(led), button_(button) {}

 private:
  mcu::OutputPin& led_;
  mcu::InputPin& button_;
};
```

## Compiler Flags

### Common Flags (All Builds)
- `-std=c++23`: C++23 standard
- `-Wall -Wextra -Werror -Wpedantic`: All warnings as errors
- `-Os`: Optimize for size
- `-g`: Debug symbols
- `-fno-rtti`: No runtime type information

### Clang-Specific
- `-stdlib=libc++`: Use LLVM's standard library
- `-Wno-c++98-compat`: Ignore C++98 compatibility warnings
- `-Wno-exit-time-destructors`, `-Wno-global-constructors`: Embedded-appropriate warnings suppressed

### ARM-Specific
- `-mthumb`: Thumb instruction set
- `-mcpu=cortex-m4` or `-mcpu=cortex-m7`: CPU architecture
- `-mfloat-abi=hard -mfpu=fpv5-d16`: FPU support (CM7)

## Key Files Reference

### Configuration
- `CMakeLists.txt` - Root build configuration
- `CMakePresets.json` - Build presets and toolchain selection
- `src/.clang-format` - Code formatting rules
- `src/.clang-tidy` - Static analysis configuration

### Core Abstractions
- `src/libs/common/error.hpp` - Error handling
- `src/libs/mcu/pin.hpp` - Pin abstraction
- `src/libs/mcu/i2c.hpp` - I2C abstraction
- `src/libs/board/board.hpp` - Board interface

### Host Implementation
- `src/libs/mcu/host/host_pin.hpp` - Host pin with ZMQ
- `src/libs/mcu/host/zmq_transport.hpp` - ZeroMQ transport
- `src/libs/mcu/host/dispatcher.hpp` - Message routing
- `src/libs/board/host/host_board.hpp` - Host board

### Example Application
- `src/apps/blinky/blinky.hpp` - LED blink app

### Testing
- `py/host-emulator/src/emulator.py` - Hardware emulator
- `py/host-emulator/tests/test_blinky.py` - Integration tests
- `py/host-emulator/tests/conftest.py` - Pytest fixtures

## Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| Host emulator | ✅ Fully working | ZMQ-based with Python emulator |
| Blinky app | ✅ Fully working | LED blink + button interrupt |
| C++ unit tests | ✅ Fully working | Transport, messages, dispatcher |
| Python integration tests | ✅ Fully working | Blinky behavior tests |
| Docker environment | ✅ Fully working | Complete dev environment |
| DevContainer | ✅ Fully working | VS Code integration |
| CI/CD | ✅ Fully working | GitHub Actions pipeline |
| STM32F3 Discovery | 🚧 Partial | Hardware files present, no C++ board impl |
| STM32F7 Nucleo | 🚧 Partial | Hardware files present, no C++ board impl |
| nRF52832 DK | ⚠️ Placeholder | Minimal setup only |

## Best Practices for AI Assistants

### DO
- ✅ Use `std::expected<T, Error>` for error handling
- ✅ Follow naming conventions strictly (clang-tidy enforces)
- ✅ Write unit tests for new MCU/board implementations
- ✅ Test on host platform first before hardware
- ✅ Use header-only implementations when possible
- ✅ Depend on abstract interfaces, not concrete types
- ✅ Use `auto` for return types to enable refactoring
- ✅ Add integration tests for new application features
- ✅ Document non-obvious design decisions in comments
- ✅ Run clang-format and clang-tidy before committing
- ✅ Use DevContainer for consistent development environment
- ✅ Ensure CI passes before merging to main

### DON'T
- ❌ Use exceptions (RTTI disabled, embedded context)
- ❌ Use raw pointers (use references or smart pointers)
- ❌ Create circular dependencies between layers
- ❌ Add dependencies without justification
- ❌ Skip tests (unit tests required for new features)
- ❌ Ignore clang-tidy warnings (they're errors)
- ❌ Use magic numbers (define named constants)
- ❌ Mix platform-specific code with abstractions
- ❌ Use `new`/`delete` directly (RAII, smart pointers)
- ❌ Add global mutable state

### When Making Changes

1. **Understand the Layer**: Determine if change affects MCU, Board, or App layer
2. **Check Existing Patterns**: Look for similar implementations
3. **Host First**: Implement and test on host platform
4. **Run Full Build**: Test both host and at least one ARM preset
5. **Verify Tests**: Ensure all tests pass (`ctest --preset=host` or `cmake --workflow --preset=host-debug`)
6. **Check Linting**: Run clang-tidy on modified files
7. **Format**: Apply clang-format before committing
8. **CI Check**: Verify GitHub Actions CI passes on your branch

### Common Tasks

**Add a new pin type**:
1. Define interface in `libs/mcu/pin.hpp`
2. Implement for host in `libs/mcu/host/host_pin.hpp`
3. Add message types to `host_emulator_messages.hpp`
4. Update Python emulator in `py/host-emulator/src/emulator.py`
5. Write tests

**Add a new peripheral (e.g., SPI)**:
1. Define interface in `libs/mcu/spi.hpp`
2. Implement host version in `libs/mcu/host/host_spi.hpp`
3. Add to board interface in `libs/board/board.hpp`
4. Implement in hardware boards
5. Add example app usage

**Debug integration test failure**:
1. Run emulator manually: `python -m py.host-emulator.src.emulator`
2. Run blinky manually: `build/host/bin/blinky`
3. Check ZMQ messages in emulator output
4. Add print statements to emulator callbacks
5. Verify JSON message format matches expectations

## Software Engineering Principles

This project demonstrates several key principles:

1. **Strong Types**: Enums and type aliases prevent errors
2. **Type Safety**: Explicit casting, no implicit conversions
3. **Compile-Time Checks**: `constexpr`, templates, concepts
4. **Correct by Construction**: APIs designed to prevent misuse
5. **Separate Calculation from Doing**: Pure functions separate from side effects
6. **Dependency Inversion**: High-level code depends on abstractions
7. **Single Responsibility**: Each class/function has one clear purpose
8. **Interface Segregation**: Small, focused interfaces
9. **Don't Repeat Yourself**: Common functionality abstracted

## Troubleshooting

### Build Issues

**Problem**: CMake can't find toolchain
```
Solution: Set CMAKE_TOOLCHAIN_PATH environment variable
export CMAKE_TOOLCHAIN_PATH=/path/to/toolchain
```

**Problem**: clang-tidy errors on correct code
```
Solution: Check .clang-tidy file, verify naming conventions
```

**Problem**: Linker errors with std::expected
```
Solution: Ensure C++23 support, check compiler version
```

### Test Issues

**Problem**: Emulator not starting
```
Solution: Check Python dependencies, run: pip install -r py/host-emulator/requirements.txt
```

**Problem**: Blinky executable not found in pytest
```
Solution: Pass --blinky flag: pytest --blinky=build/host/bin/blinky
```

**Problem**: ZMQ socket bind error
```
Solution: Kill existing processes on port, or wait for socket cleanup
```

### Runtime Issues

**Problem**: Pin operations timing out
```
Solution: Ensure emulator is running and responsive, check ZMQ connection
```

**Problem**: Interrupt handler not called
```
Solution: Verify emulator sends response messages, check dispatcher routing
```

## Resources

### Documentation
- Project README: `README.md`
- Test README: `test/README.md`
- Emulator README: `py/host-emulator/README.md`

### External References
- C++23 std::expected: https://en.cppreference.com/w/cpp/utility/expected
- ZeroMQ Guide: https://zguide.zeromq.org/
- CMake Presets: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- Embedded Template Library: https://www.etlcpp.com/

### Related Videos
- Correct by Construction: https://youtu.be/nLSm3Haxz0I
- Separate Calculating from Doing: https://youtu.be/b4p_tcLYDV0

## Version History

- **0.0.1** (Current): Initial BSP implementation with host emulation
- Multi-board support (STM32F3, STM32F7, nRF52)
- Python-based integration testing
- ZMQ transport layer

---

**Last Updated**: 2025-11-22
**CMake Version**: 3.27+
**C++ Standard**: C++23
**Docker Base**: Ubuntu 24.04 (DevContainer)
**Primary Maintainer**: Project repository owner
