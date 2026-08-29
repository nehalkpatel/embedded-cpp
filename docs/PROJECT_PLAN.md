# Embedded C++ BSP - Project Plan

**Last Updated**: 2025-11-22
**Project Status**: Educational/Demonstrative (Active Development)

## Project Vision

Explore modern C++ (C++23) and software engineering practices in embedded systems through:
- Type-safe hardware abstraction layers
- Host-based development and testing
- Correct-by-construction design patterns
- Comprehensive testing infrastructure

## Current Status

### ✅ Completed (Production Quality)

| Component | Description | Status |
|-----------|-------------|--------|
| **Host Emulation Platform** | ZeroMQ-based IPC with Python hardware simulator | ✅ Complete |
| **Blinky Example App** | LED blink + button interrupt demo | ✅ Complete |
| **UART Echo Example App** | UART RxHandler demo with async reception | ✅ Complete |
| **MCU Abstraction Layer** | Pin, UART, I2C, Delay interfaces | ✅ Complete |
| **Board Abstraction Layer** | Board interface with host implementation | ✅ Complete |
| **Error Handling** | `std::expected<T, Error>` pattern | ✅ Complete |
| **C++ Unit Tests** | Google Test for transport, messages, dispatcher | ✅ Complete |
| **Python Integration Tests** | pytest for end-to-end behavior | ✅ Complete |
| **Build System** | CMake with presets, multi-config Ninja | ✅ Complete |
| **Docker Environment** | Complete development environment | ✅ Complete |
| **DevContainer** | VS Code integration with extensions | ✅ Complete |
| **CI/CD Pipeline** | GitHub Actions for automated builds/tests | ✅ Complete |
| **Documentation** | CLAUDE.md, README.md comprehensive docs | ✅ Complete |

### 🚧 In Progress (Partial Implementation)

| Component | Status | What's Missing |
|-----------|--------|----------------|
| **STM32F3 Discovery Board** | 🚧 Partial | C++ board implementation, pin mappings |
| **STM32F7 Nucleo Board** | 🚧 Partial | C++ board implementation, pin mappings |

### ⚠️ Placeholder (Not Started)

| Component | Status | Description |
|-----------|--------|-------------|
| **nRF52832 DK Board** | ⚠️ Placeholder | Minimal CMake setup only |
| **SPI Peripheral** | ⚠️ Not Started | SPI controller interface |
| **ADC Peripheral** | ⚠️ Not Started | ADC interface |
| **PWM Peripheral** | ⚠️ Not Started | PWM interface |

## Milestones

### Milestone 1: Foundation 🚧 IN PROGRESS
**Goal**: Establish development infrastructure and prove the concept

- [x] Host emulation platform with ZeroMQ
- [x] Basic pin abstraction (Input, Output, Bidirectional)
- [x] Example application (blinky)
- [x] Unit testing framework
- [x] Integration testing with Python emulator
- [x] CMake build system with presets
- [x] Docker development environment
- [x] DevContainer for VS Code
- [x] CI/CD pipeline
- [x] Comprehensive documentation
- [x] Code coverage reporting
- [x] Add static analysis through clang-tidy by default
- [x] Add UART abstraction with RxHandler
- [x] Add I2C abstraction
- [ ] Add SPI abstraction
- [ ] Add PWM abstraction
- [ ] Add ADC abstraction
- [ ] Add async (interrupt- and DMA-driven) transfer modes to the UART and
      I2C interfaces, once a hardware platform exists that can implement them
      with genuinely different behavior than the blocking paths
- [ ] Upload code coverage reports to GitHub pages
- [ ] Increase test coverage for error paths

**Status**: 🚧 IN PROGRESS (2025-11-22)

### Milestone 2: Hardware Board Support 🚧 IN PROGRESS
**Goal**: Complete STM32F7 Nucleo board implementation

**Tasks**:
- [ ] Implement STM32F7 Nucleo C++ board class
  - [ ] Map LED pins to STM32F7 hardware
  - [ ] Map button pins to STM32F7 hardware
  - [ ] Implement GPIO initialization
  - [ ] Add interrupt handler setup
- [ ] Create STM32F7-specific pin implementations
  - [ ] Extend base pin interface for STM32 HAL
  - [ ] Handle GPIO port/pin mapping
- [ ] Test on actual hardware
  - [ ] Verify blinky builds for STM32F7
  - [ ] Flash and test LED behavior
  - [ ] Test button interrupts
- [ ] Document hardware-specific setup
  - [ ] Pin mapping tables
  - [ ] Flashing instructions
  - [ ] Debugging setup

**Success Criteria**:
- Blinky app runs on physical STM32F7 Discovery board
- All features from host emulator work on hardware
- Documentation enables others to replicate

### Milestone 3: Multi-Board Support 📋 PLANNED
**Goal**: Demonstrate portability across different MCUs

**Tasks**:
- [ ] Complete STM32F3 Discovery implementation
  - [ ] Implement board class for STM32F3
  - [ ] Map pins to Nucleo hardware
  - [ ] Test on physical hardware
- [ ] Add additional example application
  - [ ] Multi-LED pattern app
  - [ ] Demonstrates more complex behavior
- [ ] Cross-board compatibility validation
  - [ ] Ensure blinky works on both STM32F3 and STM32F7
  - [ ] Verify abstraction portability

**Success Criteria**:
- Blinky runs on both STM32F3 and STM32F7 without modification
- Additional example app demonstrates abstraction benefits

### Milestone 4: Advanced Features 🔮 FUTURE
**Goal**: Demonstrate advanced embedded patterns

**Potential Features**:
- [ ] RTOS integration (FreeRTOS)
  - [ ] Task abstraction
  - [ ] Queue/mutex abstractions
- [ ] Power management
  - [ ] Sleep modes
  - [ ] Wake-up sources
- [ ] DMA abstractions
  - [ ] Memory-to-peripheral transfers
  - [ ] Circular buffers
- [ ] Flash memory abstraction
  - [ ] Non-volatile storage
  - [ ] Configuration persistence

**Status**: Exploratory - not committed

## Current Priorities

### High Priority
1. **I2C Implementation** (Milestone 1, Priority 1)
   - Current stub needs completion
   - Demonstrates peripheral abstraction beyond GPIO

2. **Complete STM32F7 Nucleo Board** (Milestone 2)
   - Most important for proving hardware portability
   - Builds on completed foundation

### Medium Priority
3. **STM32F3 Discovery Board** (Milestone 3)
   - Proves multi-board portability
   - Demonstrates Cortex-M4 support

4. **Additional Example Applications**
   - Shows real-world patterns
   - More engaging demonstrations

### Low Priority
5. **nRF52832 DK Board**
   - Different MCU vendor (Nordic vs STM)
   - Would demonstrate even broader portability
   - Currently just placeholder

## Technical Debt & Improvements

### Code Quality
- [ ] Add more C++ unit tests for board implementations

### Documentation
- [x] ✅ Update CLAUDE.md with DevContainer setup
- [x] ✅ Refresh README.md
- [ ] Add hardware setup guides
- [ ] Add architecture diagrams

### Build System
- [x] ✅ Fix Docker permission issues
- [ ] Optimize Docker layer caching
- [ ] Add release builds to CI
- [ ] Cross-compilation verification in CI

### Host Emulator
- [ ] Add GUI visualization (instead of just console logs)
- [ ] Support for more complex I2C devices in emulator
- [ ] Timing simulation (delays, interrupt timing)

## Decision Log

### 2025-11-23: UART RxHandler Implementation
- ✅ Added UART abstraction with event-driven RxHandler (similar to Pin interrupts)
- ✅ Implemented HostUart with ZMQ transport and message routing
- ✅ Created uart_echo example app demonstrating asynchronous reception
- ✅ Added C++ unit tests for RxHandler functionality
- ✅ Created Python integration tests for uart_echo app
- ✅ UART initialization is explicit (not in Board::Init()) to avoid unnecessary emulator connections
- ✅ Improved HostBoard::Init() error handling pattern with scoped blocks
- ✅ All 20 tests passing (19 C++ + 6 Python integration)

### 2025-11-22: DevContainer & CI Integration
- ✅ Added VS Code DevContainer support
- ✅ Configured GitHub Actions CI/CD
- ✅ Resolved Docker permission issues with dynamic UID/GID
- ✅ Updated documentation (CLAUDE.md, README.md)

### 2025-11-20: Foundation Complete
- ✅ Host emulation platform working end-to-end
- ✅ Blinky example app with tests
- ✅ CMake build system with presets
- ✅ Python integration testing framework

## Success Metrics

### Educational Value
- ✅ Demonstrates modern C++ features (C++23, std::expected)
- ✅ Shows correct-by-construction patterns
- ✅ Proves host-based development viability
- 🚧 Multiple hardware boards (1 of 3 complete)

### Code Quality
- ✅ All warnings as errors
- ✅ clang-tidy enforcement
- ✅ Comprehensive testing (unit + integration)
- ✅ CI/CD automation

### Developer Experience
- ✅ Easy setup (DevContainer)
- ✅ Fast iteration (host builds)
- ✅ Clear documentation
- 🚧 Hardware debugging setup (not yet documented)

## Resources

### Reference Videos
- [Correct-by-Construction](https://youtu.be/nLSm3Haxz0I) - Design philosophy
- [Separate Calculating from Doing](https://youtu.be/b4p_tcLYDV0) - Architecture pattern

### Technologies
- [CMake](https://cmake.org/) - Build system
- [Embedded Template Library](https://www.etlcpp.com/) - STL alternative for embedded
- [ZeroMQ](https://zeromq.org/) - IPC transport
- [Google Test](https://github.com/google/googletest) - C++ testing
- [pytest](https://pytest.org/) - Python testing

---
