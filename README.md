# Embedded C++ Board Support Package (BSP)

A modern C++23 embedded systems project demonstrating best practices for hardware abstraction, host-based development, and automated testing.

## Overview

This project explores:
1. **Modern C++ in Embedded Systems** - Applying C++23 features and software engineering principles to microcontroller development
2. **Host-Side Simulation** - Desktop development and testing environment with Python-based hardware emulation
3. **Correct-by-Construction Design** - Type-safe abstractions and compile-time verification

**Status**: Educational/demonstrative project (not production-ready)

## Key Features

- ✅ **Multi-Platform Support**: Host (x86_64), ARM Cortex-M4, ARM Cortex-M7
- ✅ **Hardware Abstraction Layers**: Clean separation between MCU, Board, and Application layers
- ✅ **Host Emulation**: ZeroMQ-based IPC with Python hardware simulator
- ✅ **Comprehensive Testing**: C++ unit tests (Google Test) + Python integration tests (pytest)
- ✅ **DevContainer Support**: Full VS Code DevContainer configuration
- ✅ **CI/CD Pipeline**: GitHub Actions for automated builds and tests
- 🚧 **Multi-Board**: STM32F3 Discovery, STM32F7 Nucleo, nRF52832 DK (partial implementation)

## Quick Start

### Option 1: VS Code DevContainer (Recommended)

1. Install [VS Code](https://code.visualstudio.com/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open this repository in VS Code
3. Press `Ctrl+Shift+P` and select "Dev Containers: Reopen in Container"
4. Wait for the container to build (first time only)
5. Open a terminal and run:
   ```bash
   cmake --workflow --preset=host-debug
   ```

That's it! The DevContainer includes all tools pre-installed.

### Option 2: Docker Compose

```bash
# Clone the repository
git clone <repository-url>
cd embedded-cpp

# Run host-debug workflow (build + test)
docker compose run --rm host-debug

# Or run host-release workflow
docker compose run --rm host-release
```

### Option 3: Local Build

**Requirements**:
- CMake 3.27+
- Ninja
- Clang 18+ (host builds) or ARM GCC (embedded builds)
- Python 3.10+ (for integration tests)
- ZeroMQ (libzmq3-dev)

```bash
# Configure for host platform
cmake --preset=host

# Build (Debug)
cmake --build --preset=host --config Debug

# Run tests
ctest --preset=host -C Debug --output-on-failure

# Or use workflow preset (configure + build + test)
cmake --workflow --preset=host-debug
```

## Project Structure

```
embedded-cpp/
├── src/
│   ├── apps/blinky/          # Example LED blink application
│   ├── libs/
│   │   ├── common/           # Error handling (std::expected)
│   │   ├── mcu/              # MCU abstraction (Pin, I2C, Delay)
│   │   │   └── host/         # Host emulation with ZeroMQ
│   │   └── board/            # Board abstraction (LEDs, buttons, peripherals)
│   │       ├── host/         # Host board implementation
│   │       ├── stm32f3_discovery/
│   │       ├── stm32f767zi_nucleo/
│   │       └── nrf52832_dk/
│
├── py/host-emulator/         # Python hardware emulator
│   ├── src/emulator.py       # Virtual device simulator
│   └── tests/                # Integration tests (pytest)
│
├── cmake/toolchain/          # Cross-compilation toolchains
├── .devcontainer/            # VS Code DevContainer config
├── .github/workflows/        # GitHub Actions CI/CD
└── CLAUDE.md                 # Comprehensive project documentation
```

## Technology Stack

| Category | Technology |
|----------|------------|
| **Language** | C++23 |
| **Build System** | CMake 3.27+ with Ninja Multi-Config |
| **Compilers** | Clang 18 (host), ARM GCC (embedded) |
| **Testing** | Google Test (C++), pytest (Python) |
| **IPC** | ZeroMQ with JSON serialization |
| **Embedded Targets** | STM32F3, STM32F7, nRF52832 |
| **Architectures** | ARM Cortex-M4, ARM Cortex-M7 |
| **Development** | Docker, VS Code DevContainers |
| **CI/CD** | GitHub Actions |

## Building for Different Platforms

```bash
# Host platform (development/testing)
cmake --workflow --preset=host-debug
cmake --workflow --preset=host-release

# STM32F3 Discovery (ARM Cortex-M4)
cmake --workflow --preset=stm32f3_discovery-release

# ARM Cortex-M7 (base preset)
cmake --workflow --preset=arm-cm7-release
```

## Running Tests

### C++ Unit Tests

```bash
# Run all tests
ctest --preset=host -C Debug --output-on-failure

# Run specific test
ctest --preset=host -C Debug -R test_zmq_transport
```

### Python Integration Tests

```bash
cd py/host-emulator

# Install dependencies
pip install -r requirements.txt

# Run tests (requires built blinky executable)
pytest tests/ --blinky=../../build/host/bin/blinky
```

## Example Application: Blinky

The `blinky` application demonstrates:
- LED blinking at 500ms intervals
- Button interrupt handling (rising edge detection)
- Dependency injection with board abstraction
- Error handling with `std::expected`

**Run on host emulator**:
```bash
# Terminal 1: Start Python emulator
cd py/host-emulator
python -m src.emulator

# Terminal 2: Run blinky
cd build/host/bin
./blinky
```

The emulator will print pin state changes as the application runs.

## Software Engineering Principles

This project demonstrates:

1. **Strong Types** - Enums and type aliases prevent errors (PinState, PinDirection, Error)
2. **Type Safety** - Explicit casting, no implicit conversions
3. **Compile-Time Checks** - `constexpr`, templates, concepts
4. **Correct by Construction** - APIs designed to prevent misuse
5. **Separate Calculation from Doing** - Pure functions separate from side effects
6. **Dependency Inversion** - High-level code depends on abstractions
7. **Single Responsibility** - Each class/function has one clear purpose
8. **Interface Segregation** - Small, focused interfaces (InputPin, OutputPin, BidirectionalPin)
9. **Error Handling** - `std::expected<T, Error>` instead of exceptions (RTTI disabled)

See [Correct-by-Construction](https://youtu.be/nLSm3Haxz0I) and [Separate Calculating from Doing](https://youtu.be/b4p_tcLYDV0) for more details.

## Code Quality

- **Formatting**: `.clang-format` (Google style, pointer-left alignment)
- **Linting**: `.clang-tidy` (comprehensive checks with naming convention enforcement)
- **Warnings**: All warnings are errors (`-Werror`)
- **Standard**: C++23 with strict compliance
- **RTTI**: Disabled (no exceptions, embedded-friendly)

## Development Workflow

1. **Work in DevContainer** (or use Docker)
2. **Implement on Host First** - Faster iteration, easier debugging
3. **Write Tests** - Unit tests (C++) and integration tests (Python)
4. **Run Linting** - `clang-tidy` enforces conventions
5. **Format Code** - `clang-format` on modified files
6. **Verify CI** - Ensure GitHub Actions passes
7. **Port to Hardware** - Test on actual embedded boards

## Documentation

- **[CLAUDE.md](CLAUDE.md)** - Comprehensive project documentation (architecture, conventions, workflows)
- **[test/README.md](test/README.md)** - Testing infrastructure details
- **[py/host-emulator/README.md](py/host-emulator/README.md)** - Python emulator documentation

## Contributing

This is an educational project. If you'd like to contribute or experiment:

1. Use the DevContainer for a consistent environment
2. Follow the coding conventions (enforced by clang-tidy)
3. Write tests for new features
4. Ensure CI passes before submitting PRs
5. See [CLAUDE.md](CLAUDE.md) for detailed guidelines

## Implementation Status

| Component | Status |
|-----------|--------|
| Host emulation | ✅ Fully working |
| Blinky app | ✅ Fully working |
| C++ unit tests | ✅ Fully working |
| Python integration tests | ✅ Fully working |
| Docker/DevContainer | ✅ Fully working |
| CI/CD | ✅ Fully working |
| STM32F3 Discovery | 🚧 Partial (hardware files present) |
| STM32F7 Nucleo | 🚧 Partial (hardware files present) |
| nRF52832 DK | ⚠️ Placeholder only |

## License

See LICENSE file for details.

## Acknowledgments

- Built with [CMake](https://cmake.org/), [Embedded Template Library](https://www.etlcpp.com/), [ZeroMQ](https://zeromq.org/)
- Inspired by modern C++ embedded practices and correct-by-construction design
