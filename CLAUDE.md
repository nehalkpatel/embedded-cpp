# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Full workflow: configure + build + test (recommended)
cmake --workflow --preset=host-debug
cmake --workflow --preset=host-release

# Manual steps
cmake --preset=host                          # Configure
cmake --build --preset=host-debug            # Build
ctest --preset=host-debug                    # Test all

# Run single C++ test
ctest --preset=host-debug -R ZmqTransportTest

# Run all Python integration tests (wires up executable paths for you)
ctest --preset=host-debug -R host_emulator_test

# Run a single Python integration test directly (needs the app path;
# --extra dev pulls in pytest, which lives in the dev optional-dependency group)
cd py/host-emulator && uv run --extra dev pytest tests/test_blinky.py -v \
    --blinky=../../build/host/bin/Debug/blinky

# Python lint / type-check
cd py/host-emulator && uv run --extra dev ruff check . && uv run --extra dev mypy src

# Cross-compile for ARM - not yet functional. Presets and toolchain files exist,
# but src/libs/mcu/CMakeLists.txt does add_subdirectory(${EMBEDDED_CPP_MCU}) and
# only the `host` implementation exists, so configure fails on the missing
# arm_cm4/ directory. Host build and emulation come first; hardware follows.
cmake --workflow --preset=stm32f3_discovery-release

# Docker alternative
docker compose run --rm host-debug
```

## Architecture

**Layered design with dependency inversion** - upper layers depend on abstract interfaces, platform selected at CMake time:

```
Application (apps/)  →  Board (libs/board/)  →  MCU (libs/mcu/)  →  Platform Implementations
```

**Host emulation**: C++ apps communicate with Python hardware emulator via ZeroMQ/JSON IPC. This enables desktop development and integration testing without hardware.

## Key Constraints

- **No exceptions** - RTTI disabled; use `std::expected<T, common::Error>` for all fallible operations
- **No raw pointers** - use references or smart pointers
- **No `new`/`delete`** - use RAII and smart pointers
- **clang-tidy enforced** - build fails on violations; naming conventions strictly enforced:
  - `PascalCase`: Classes, structs, functions, methods, enum values (prefixed with `k`)
  - `snake_case`: Namespaces, variables, private members (with trailing `_`)

## Code Patterns

Error handling:
```cpp
auto result = SomeOperation();
if (!result) {
  return std::unexpected(result.error());
}
// Use result.value()
```

Dependency injection via abstract interfaces:
```cpp
class MyApp {
 public:
  MyApp(mcu::OutputPin& led) : led_(led) {}
 private:
  mcu::OutputPin& led_;
};
```

## Adding New Features

1. Define interface in `libs/mcu/*.hpp` (for peripherals) or `libs/board/board.hpp`
2. Implement host version in `libs/mcu/host/` with ZMQ messaging
3. Add message types to `host_emulator_messages.hpp`
4. Update Python emulator in `py/host-emulator/src/host_emulator/`
5. Write unit tests (C++) and integration tests (Python)
6. Implement hardware versions in board-specific directories

## Testing

- **C++ unit tests**: Colocated with code (`src/libs/mcu/host/test_*.cpp`), use Google Test
- **Python integration tests**: `py/host-emulator/tests/`, use pytest with fixtures that manage emulator/app lifecycle. CTest builds a uv venv under `build/host/host_emulator_venv` and runs them as the `host_emulator_test` target
- **clang-tidy**: Runs automatically during build, no separate step needed
- **Python tooling**: uv + ruff + strict mypy, all configured in `py/host-emulator/pyproject.toml`

## Important Files

- `src/libs/common/error.hpp` - Error enum and `std::expected` usage
- `src/libs/mcu/pin.hpp` - Pin abstraction (InputPin, OutputPin, BidirectionalPin)
- `src/libs/mcu/uart.hpp` - UART with RxHandler callback pattern
- `src/libs/board/board.hpp` - Board interface aggregating all peripherals
- `CMakePresets.json` - Build configurations for host and ARM targets
