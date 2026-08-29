# Embedded C++ BSP - Project Plan

**Last Updated**: 2026-08-29
**Project Status**: Educational/Demonstrative (Active Development)

This document is forward-looking: milestones, priorities, and open work.
Current implementation status lives in the README's Implementation Status
table; what exists is best read from the code and `ctest -N`.

## Project Vision

Explore modern C++ (C++23) and software engineering practices in embedded
systems through:
- Type-safe hardware abstraction layers
- Host-based development and testing
- Correct-by-construction design patterns
- Comprehensive testing infrastructure

## Milestones

### Milestone 1: Foundation 🚧 IN PROGRESS

**Goal**: Establish development infrastructure and prove the concept

- [x] Host emulation platform with ZeroMQ
- [x] Basic pin abstraction (Input, Output, Bidirectional)
- [x] Example applications (blinky, uart_echo, i2c_demo)
- [x] Unit testing framework
- [x] Integration testing with Python emulator
- [x] CMake build system with presets
- [x] Docker development environment
- [x] DevContainer for VS Code
- [x] CI/CD pipeline
- [x] Code coverage reporting
- [x] Static analysis through clang-tidy by default
- [x] UART abstraction with RxHandler
- [x] I2C abstraction
- [x] Wire-protocol documentation (py/host-emulator/README.md)
- [ ] Add SPI abstraction
- [ ] Add PWM abstraction
- [ ] Add ADC abstraction
- [ ] Add async (interrupt- and DMA-driven) transfer modes to the UART and
      I2C interfaces, once a hardware platform exists that can implement them
      with genuinely different behavior than the blocking paths
- [ ] Upload code coverage reports to GitHub pages
- [ ] Increase test coverage for error paths

### Milestone 2: Hardware Board Support 📋 PLANNED

**Goal**: First physical board (STM32F7 Nucleo)

The repository currently carries only the ARM toolchain files and configure
presets; the vendor HAL trees that briefly lived in-tree were removed while
unreachable (git history preserves them, and CubeMX regenerates them fresher).

**Tasks**:
- [ ] Implement the `arm_cm7` MCU backend (`src/libs/mcu/arm_cm7/`)
- [ ] Implement the STM32F7 Nucleo board directory (pin maps, GPIO init,
      interrupt wiring) against the vendor HAL
- [ ] Verify blinky builds, flashes, and runs on the physical board
- [ ] Document hardware setup: pin mapping tables, flashing, debugging

**Success Criteria**:
- Blinky runs on a physical STM32F7 Nucleo board
- All features exercised by the host emulator work on hardware
- Documentation enables others to replicate

### Milestone 3: Multi-Board Support 📋 PLANNED

**Goal**: Demonstrate portability across different MCUs

**Tasks**:
- [ ] STM32F3 Discovery support (`arm_cm4` backend + board directory)
- [ ] Additional example application exercising more complex behavior
- [ ] Cross-board validation: blinky runs on both boards unmodified

### Milestone 4: Advanced Features 🔮 FUTURE

**Potential Features** (exploratory, not committed):
- RTOS integration (FreeRTOS): task, queue, and mutex abstractions
- Power management: sleep modes, wake-up sources
- DMA abstractions: memory-to-peripheral transfers, circular buffers
- Flash memory abstraction: non-volatile storage, config persistence
- nRF52832 DK board (a second silicon vendor)

## Current Priorities

1. **Complete STM32F7 Nucleo board** (Milestone 2) — proves hardware
   portability, builds on the completed host foundation
2. **STM32F3 Discovery board** (Milestone 3) — proves multi-board portability
3. **Additional example applications** — more engaging demonstrations

## Technical Debt & Improvements

- [ ] Add C++ unit tests for board implementations
- [ ] Add hardware setup guides and architecture diagrams
- [ ] Optimize Docker layer caching
- [ ] Add release builds to CI
- [ ] Cross-compilation verification in CI (once an ARM backend exists)
- [ ] Host emulator: GUI visualization, richer I2C device models, timing
      simulation
- [ ] Wire up Python test coverage if it earns its keep (pytest-cov was
      removed while unused)

## Decision Log

### 2026-08-29: Simplification pass
- Codebase-wide review against the project's educational goals; the themes:
  one canonical form per idea (a single Transact/Peripheral implementation
  instead of three divergent copies), target-based CMake usage requirements,
  and docs that match behavior
- Trimmed Uart/I2C to the surface the host honors; async/interrupt/DMA modes
  recorded above as future work
- Parked the unreachable STM32 vendor trees and broken ARM workflow presets;
  kept toolchains and configure presets behind a clear "not implemented"
  configure error

### 2025-11-23: UART RxHandler implementation
- UART abstraction with event-driven RxHandler (similar to pin interrupts),
  HostUart with ZMQ transport and message routing, uart_echo example app,
  C++ unit tests and Python integration tests
- UART initialization is explicit (not in Board::Init()) to avoid unnecessary
  emulator connections

### 2025-11-22: DevContainer & CI integration
- VS Code DevContainer support, GitHub Actions CI/CD, Docker permission
  handling, documentation updates

### 2025-11-20: Foundation complete
- Host emulation platform working end-to-end; blinky with tests; CMake preset
  build system; Python integration testing framework

## Resources

### Reference Videos
- [Correct-by-Construction](https://youtu.be/nLSm3Haxz0I) - Design philosophy
- [Separate Calculating from Doing](https://youtu.be/b4p_tcLYDV0) - Architecture pattern

### Technologies
- [CMake](https://cmake.org/) - Build system
- [ZeroMQ](https://zeromq.org/) - IPC transport
- [Google Test](https://github.com/google/googletest) - C++ testing
- [pytest](https://pytest.org/) - Python testing
- [Embedded Template Library](https://www.etlcpp.com/) - STL alternative to
  consider when hardware targets arrive (not currently a dependency)
