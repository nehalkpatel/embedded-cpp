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

# Run a single Python integration test directly (needs the app path; pytest comes
# from the PEP 735 dev dependency group, which uv syncs by default)
cd py/host-emulator && uv run pytest tests/test_blinky.py -v \
    --blinky=../../build/host/bin/Debug/blinky

# Formatting (C++ and Python). CI, the pre-commit hook, and these targets all
# call tools/format.sh, so they cannot disagree about what "formatted" means.
tools/format.sh --fix                        # Reformat in place
tools/format.sh --check                      # Verify, exactly as CI does
cmake --build build/host --target format     # Same, via CMake
cmake --build build/host --target format-check

# The pre-commit hook is installed by the CMake configure step, which points
# core.hooksPath at .githooks. Opt out with -DINSTALL_GIT_HOOKS=OFF; bypass a
# single commit with `git commit --no-verify`.

# Python type-check (not covered by format.sh - types are not formatting)
cd py/host-emulator && uv run mypy

# Cross-compile for ARM - not yet functional. Toolchain files and configure
# presets exist, but only the `host` MCU/board implementations do; configuring
# an ARM preset stops with a message saying the backend is not implemented.
# Host build and emulation come first; hardware follows.
cmake --preset=stm32f3_discovery

# Docker alternative
docker compose run --rm embedded-cpp-dev cmake --workflow --preset host-debug
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
3. Add message types to `host_emulator_messages.hpp` and their JSON tables to `emulator_message_json_encoder.hpp`
4. Update the Python emulator in `py/host-emulator/src/host_emulator/`. The wire protocol is documented in `py/host-emulator/README.md`; the C++ and Python vocabularies mirror each other and must change together
5. Write unit tests (C++) and integration tests (Python)
6. Implement hardware versions in board-specific directories

## Testing

- **C++ unit tests**: Colocated with code (`src/libs/mcu/host/test_*.cpp`), use Google Test
- **Python integration tests**: `py/host-emulator/tests/`, use pytest with fixtures that manage emulator/app lifecycle. They run as the `host_emulator_test` CTest target; a CTest setup fixture syncs a uv venv under `build/host/host_emulator_venv` first (a no-op once synced)
- **System tests**: none yet — end-to-end coverage lives in the Python integration tests. Add a dedicated harness only when a test doesn't fit the emulator harness
- **clang-tidy**: Runs automatically during build, no separate step needed
- **Python tooling**: uv + ruff + strict mypy, all configured in `py/host-emulator/pyproject.toml`

## Important Files

- `src/libs/common/error.hpp` - Error enum and `std::expected` usage
- `src/libs/mcu/pin.hpp` - Pin abstraction (InputPin, OutputPin, BidirectionalPin)
- `src/libs/mcu/uart.hpp` - UART with RxHandler callback pattern
- `src/libs/board/board.hpp` - Board interface aggregating all peripherals
- `py/host-emulator/README.md` - The ZeroMQ/JSON wire protocol (canonical doc)
- `CMakePresets.json` - Build configurations for host and ARM targets
