# Embedded CPP BSP

[![CI](https://github.com/nehalkpatel/embedded-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/nehalkpatel/embedded-cpp/actions/workflows/ci.yml)

The purpose of this project is to explore:
1. Using modern C++ and software engineering practices and their applications to embedded systems.
1. Building a host-side simulation and visualization environment.

## C++ Features and SW Engineering Principles
1. Strong types and type-safety
1. Explicit casting and conversion
1. `std::expected` to return values or errors
1. Compile-time checks
1. [Correct-by-construction](https://youtu.be/nLSm3Haxz0I?si=Q-PyVHR4wj7ozaEG)
1. [Separate calculating from doing](https://youtu.be/b4p_tcLYDV0?si=knDDTrq1k3dvHvu0)

## Quick Start

### Prerequisites

- **CMake**: 3.27 or higher
- **Clang/LLVM**: 14 or higher (for host builds)
- **Python**: 3.11 or higher
- **Ninja**: Build system
- **ARM GCC**: For embedded target builds (optional)

### Building for Host (Development/Testing)

```bash
# Configure for host platform
cmake --preset=host

# Build (Debug or Release)
cmake --build --preset=host --config Debug

# Run tests
ctest --preset=host -C Debug --output-on-failure
```

### Running the Blinky Example

```bash
# Start the Python emulator in one terminal
cd py/host-emulator
python -m src.emulator

# Run the blinky application in another terminal
./build/host/bin/blinky
```

### Building for Hardware

```bash
# Configure for STM32F3 Discovery
cmake --preset=stm32f3_discovery

# Build
cmake --build --preset=stm32f3_discovery --config Release
```

## Project Structure

- `src/libs/mcu/` - MCU abstraction layer (Pin, I2C, Delay interfaces)
- `src/libs/board/` - Board abstraction layer (Board interface)
- `src/apps/` - Application layer (Blinky example)
- `py/host-emulator/` - Python-based hardware emulator
- `test/` - System-level tests

## Documentation

For comprehensive documentation on the codebase structure, architecture, and development workflows, see [CLAUDE.md](CLAUDE.md).

## Running Tests

### C++ Unit Tests

```bash
# Run all unit tests
ctest --preset=host -C Debug --output-on-failure

# Run specific test
ctest --preset=host -C Debug -R test_zmq_transport
```

### Python Integration Tests

```bash
cd py/host-emulator
pip install -r requirements.txt
pytest tests/ --blinky=../../build/host/bin/blinky -v
```

## Supported Platforms

- **Host** (Linux, macOS): Full support with emulation
- **STM32F3 Discovery**: Partial support (in progress)
- **STM32F767 Nucleo**: Planned
- **nRF52832 DK**: Planned