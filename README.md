# Embedded C++ Board Support Package (BSP)

A modern C++23 embedded systems project demonstrating best practices for hardware abstraction, host-based development, and automated testing.

## Overview

This project explores:
- **Modern C++ in Embedded Systems** - C++23 features and software engineering principles for microcontrollers
- **Host-Side Simulation** - Desktop development with Python-based hardware emulation via ZeroMQ
- **Correct-by-Construction Design** - Type-safe abstractions and compile-time verification

**Status**: Educational/demonstrative project (not production-ready)

## Quick Start

### VS Code DevContainer (Recommended)

1. Install [VS Code](https://code.visualstudio.com/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open this repository in VS Code
3. Press `Ctrl+Shift+P` → "Dev Containers: Reopen in Container"
4. Run: `cmake --workflow --preset=host-debug`

### Docker Compose

```bash
docker compose run --rm host-debug
```

### Local Build

**Requirements**: CMake 3.27+, Ninja, Clang 18+, Python 3.11+, [uv](https://docs.astral.sh/uv/), ZeroMQ (libzmq3-dev)

```bash
cmake --workflow --preset=host-debug    # Configure + build + test
```

If your compilers aren't on `PATH` at the expected versions (common on macOS with
Homebrew LLVM), copy `CMakeUserPresets.json.example` to `CMakeUserPresets.json`
and edit the paths. That file is gitignored, so it stays machine-local.

## Architecture

```
Application (apps/)  →  Board (libs/board/)  →  MCU (libs/mcu/)  →  Platform Implementations
```

- **apps/**: Example applications (blinky, uart_echo, i2c_demo)
- **libs/mcu/**: Hardware abstractions (Pin, UART, I2C, Delay) with host emulation
- **libs/board/**: Board-specific implementations (host, STM32F3, STM32F7, nRF52)
- **py/host-emulator/**: Python hardware simulator for desktop testing

## Build Commands

```bash
# Host (development/testing)
cmake --workflow --preset=host-debug
cmake --workflow --preset=host-release

# ARM targets - not yet functional (see Implementation Status below).
# The presets and toolchain files are in place, but configuring fails until
# the MCU layer lands in src/libs/mcu/arm_cm4/ (and arm_cm7/ for the F7).
cmake --workflow --preset=stm32f3_discovery-release
```

## Running Tests

```bash
# All tests
ctest --preset=host-debug

# Single C++ test
ctest --preset=host-debug -R ZmqTransportTest

# Python integration tests (via CTest, which supplies the app paths)
ctest --preset=host-debug -R host_emulator_test
```

## Example: Running Blinky

```bash
# Terminal 1: Start emulator
cd py/host-emulator && uv run python -m host_emulator.emulator

# Terminal 2: Run application
./build/host/bin/Debug/blinky
```

## Technology Stack

| Category | Technology |
|----------|------------|
| Language | C++23 |
| Build | CMake 3.27+ / Ninja |
| Compilers | Clang 18 (host), ARM GCC (embedded) |
| Testing | Google Test, pytest |
| IPC | ZeroMQ + JSON |
| Targets | STM32F3, STM32F7, nRF52832 |

## Code Quality

- **No exceptions** - Uses `std::expected<T, Error>` (RTTI disabled)
- **clang-tidy** - Enforced during build with strict naming conventions
- **clang-format** - Google style with left pointer alignment
- **-Werror** - All warnings are errors

## Implementation Status

| Component | Status |
|-----------|--------|
| Host emulation | ✅ Working |
| Example apps | ✅ Working |
| C++ unit tests | ✅ Working |
| Python integration tests | ✅ Working |
| Docker/DevContainer | ✅ Working |
| CI/CD | ✅ Working |
| STM32F3/F7 | 🚧 Partial |
| nRF52832 | ⚠️ Placeholder |

## Resources

- [Correct-by-Construction](https://youtu.be/nLSm3Haxz0I)
- [Separate Calculating from Doing](https://youtu.be/b4p_tcLYDV0)
