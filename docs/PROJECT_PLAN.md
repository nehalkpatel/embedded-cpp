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

The backend, the board and a linked firmware image exist; the drivers are
written against CMSIS register definitions rather than the Cube HAL, so the
clock ordering and bit layouts stay visible in the code. Hardware verification
is the open part.

**Tasks**:
- [x] Implement the `arm_cm7` MCU backend (`src/libs/mcu/arm_cm7/`)
- [x] Implement the STM32F7 Nucleo board directory (pin map, GPIO, EXTI,
      SysTick) against CMSIS register definitions
- [x] Cross-compile verification without hardware: image layout, entry point,
      static-constructor array, undefined symbols, no allocation on interrupt
      paths (`tools/verify-firmware.sh`, run in CI)
- [x] Document hardware setup: pin mapping tables, flashing, debugging
      (`docs/HARDWARE.md`)
- [ ] Verify blinky flashes and runs on the physical board
- [ ] USART3 to the ST-LINK virtual COM port, replacing the placeholder
- [ ] I2C1 on PB8/PB9, replacing the placeholder
- [ ] Replace `nosys.specs` with real newlib syscalls once there is a UART to
      retarget `_write` to, and a `_sbrk` bounded by the linker script's
      `__heap_limit`

**Success Criteria**:
- Blinky runs on a physical STM32F7 Nucleo board
- All features exercised by the host emulator work on hardware
- Documentation enables others to replicate

### Milestone 3: Multi-Board Support 📋 PLANNED

**Goal**: Demonstrate portability across different MCUs

**Tasks**:
- [ ] STM32F3 Discovery support (`arm_cm4` backend + board directory)
- [ ] Revisit `board::Board`'s all-or-nothing interface now that a second
      hardware board exists: optional accessors
      (`std::expected<mcu::I2CController&, Error>`) vs. capability mix-ins vs.
      compile-time board traits. Deferred from Milestone 2 deliberately — with
      one board there was nothing to design against.
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

1. **Verify blinky on the physical F767ZI**, then USART3 and I2C1
   (Milestone 2) — the backend and board exist and the image is verified by
   cross-build; what remains needs the board in hand
2. **STM32F3 Discovery board** (Milestone 3) — proves multi-board portability,
   and is the first data point for narrowing `board::Board`
3. **Additional example applications** — more engaging demonstrations

## Technical Debt & Improvements

- [ ] Add C++ unit tests for board implementations
- [ ] Add hardware setup guides and architecture diagrams
- [ ] Optimize Docker layer caching
- [ ] Add release builds to CI
- [ ] Host emulator: GUI visualization, richer I2C device models, timing
      simulation
- [ ] Wire up Python test coverage if it earns its keep (pytest-cov was
      removed while unused)

## Decision Log

### 2026-09-04: First hardware backend (arm_cm7 + F767ZI Nucleo)

- Chose CMSIS device headers over the Cube HAL. The 30k lines of register
  #defines are a mechanical transcription of RM0410 and teach nothing, but
  RCC enable ordering, the MODER/OTYPER/AFR layout and the EXTI/SYSCFG/NVIC
  chain are exactly what `HAL_GPIO_Init()` hides -- and full CubeF7 is ~1 GB
  of middleware for a project that wants the registers visible
- No public header in the backend names a vendor type. CMSIS defines `I2C1`,
  `USART3` and hundreds of other bare identifiers as macros; the first one to
  bite rewrote `board::Board::I2C1()` into a syntax error. Pins name their
  port with `mcu::GpioPort` instead, and `cmsis.hpp` is included only from
  .cpp files
- Wrote the linker script rather than recovering the Ac6 one from history:
  that file forbids redistribution, sizes RAM at 320K (F746 numbers, not the
  F767ZI's 512K), and discards every section of libc.a, libm.a and libgcc.a
- Kept `startup.s` from history (ST, BSD-3) for its 110-entry vector table,
  with the contradictory `.fpu softvfp` removed
- Stayed on the distro's `gcc-arm-none-eabi` 13.2. Verified that every
  portable header and app source compiles at `-std=c++23`; libstdc++ 13's
  missing `<print>` affects only two host-only translation units
- `-fno-exceptions` is now real, and cross-build-only: the host entry point
  is an exception boundary by design because cppzmq throws
- Deferred the PLL, the caches and the ART accelerator. Each can break
  working peripherals, and each deserves a known-good baseline to regress
  against

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
