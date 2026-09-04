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
docker compose run --rm embedded-cpp-dev cmake --workflow --preset host-debug
```

### Local Build

**Requirements**: CMake 3.27+, Ninja, Clang 18+, Python 3.14+, [uv](https://docs.astral.sh/uv/), ZeroMQ (libzmq3-dev)

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
- **libs/board/**: Board-specific implementations (host today; hardware boards planned)
- **py/host-emulator/**: Python hardware simulator for desktop testing

## Build Commands

```bash
# Host (development/testing)
cmake --workflow --preset=host-debug
cmake --workflow --preset=host-release

# STM32F767ZI Nucleo (Cortex-M7). Configure and build only: firmware has no
# tests that run on the build machine, so nothing here runs ctest. See
# docs/HARDWARE.md for flashing, debugging and the pin map.
cmake --workflow --preset=nucleo-f767zi-debug
cmake --workflow --preset=nucleo-f767zi-release

# Structural checks on the linked image -- vector table address, entry point,
# static-constructor array, undefined symbols. No hardware needed; CI runs it.
tools/verify-firmware.sh build/nucleo-f767zi

# Other ARM presets are toolchain-only: no arm_cm4 backend or F3 Discovery
# board exists yet, so configuring stops with a message naming what does.
cmake --preset=stm32f3_discovery
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
cd py/host-emulator && uv run host-emulator

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
| Targets | Host emulation (hardware targets planned) |

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
| ARM cross-compile (Cortex-M7) | ✅ Working |
| STM32F767ZI Nucleo: GPIO, EXTI, SysTick | ✅ Working |
| STM32F767ZI Nucleo: UART, I2C | 🚧 Placeholders that return an error |
| Other boards (STM32F3, nRF52) | 📋 Planned |

## Resources

- [Correct-by-Construction](https://youtu.be/nLSm3Haxz0I)
- [Separate Calculating from Doing](https://youtu.be/b4p_tcLYDV0)
