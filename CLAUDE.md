# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Full workflow: configure + build + test (recommended)
cmake --workflow --preset=host-debug
cmake --workflow --preset=host-release

# Manual steps
cmake --preset=host                          # Configure
cmake --build --preset=host --config Debug   # Build
ctest --preset=host -C Debug                 # Test all

# Run single C++ test
ctest --preset=host -C Debug -R test_zmq_transport

# Run single Python integration test
cd py/host-emulator && pytest tests/test_blinky.py -v

# Cross-compile for ARM
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
4. Update Python emulator in `py/host-emulator/src/`
5. Write unit tests (C++) and integration tests (Python)
6. Implement hardware versions in board-specific directories

## Testing

- **C++ unit tests**: Colocated with code (`src/libs/mcu/host/test_*.cpp`), use Google Test
- **Python integration tests**: `py/host-emulator/tests/`, use pytest with fixtures that manage emulator/app lifecycle
- **clang-tidy**: Runs automatically during build, no separate step needed

## Important Files

- `src/libs/common/error.hpp` - Error enum and `std::expected` usage
- `src/libs/mcu/pin.hpp` - Pin abstraction (InputPin, OutputPin, BidirectionalPin)
- `src/libs/mcu/uart.hpp` - UART with RxHandler callback pattern
- `src/libs/board/board.hpp` - Board interface aggregating all peripherals
- `CMakePresets.json` - Build configurations for host and ARM targets
