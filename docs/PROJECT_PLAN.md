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
- [x] Verify blinky flashes and runs on the physical board — LD1 toggles at a
      measured 200 ms and B1 latches LD2 from an EXTI handler, from unmodified
      `blinky.cpp` (2026-09-04)
- [x] USART3 to the ST-LINK virtual COM port, replacing the placeholder
- [x] I2C1 on PB8/PB9, replacing the last placeholder
- [ ] Verify `i2c_demo` on hardware (NACK path without a device; round trip
      with one)
- [x] Replace `nosys.specs` with real newlib syscalls: `_write` retargeted to
      USART3, `_sbrk` bounded by the linker script's `__heap_limit`, and
      `__malloc_lock` overridden so an allocating interrupt handler cannot
      re-enter the allocator
- [x] Verify `uart_echo` over the virtual COM port on hardware — greeting
      and per-character echo confirmed at 115200 (2026-09-04)

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
      one board there was nothing to design against. No longer entangled with
      #37: peripherals now configure themselves at construction, so there is no
      "did not come up" state for an accessor to report.
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

### 2026-09-07: Boot flow named in two stages, split on fallibility

- Wrote `docs/BOOT_FLOW.md`. The stage a peripheral belongs to is decided by
  **what can fail**, not by what is convenient: on-chip bring-up is register
  writes against hardware soldered into the die, so it cannot fail and lives in
  a constructor; off-chip bring-up is I/O across a wire, where absence and
  silence are normal outcomes, so it needs somewhere that can report one --
  `Board::Init()`.
- The structure this describes already existed; what was missing was the
  contract. `board::Board` now states it where a reader meets it.
- Corrected a claim made in the 2026-09-06 entry above. `mcu::Delay` *does*
  work before the tick -- it falls back to a `DWT->CYCCNT` spin -- so
  "bring-up must not call mcu::Delay" was wrong. The real boundary is narrower
  and sharper: `Millis()` is frozen until `InitSysTick()`, so a *bounded* wait
  can never end. Since every real bus transfer needs a timeout, stage 1 may
  wait but may not talk.
- That was a live latent hang, not just a doc error. `I2CBus`'s flag wait and
  `Usart::Receive` compared against a frozen `Millis()` with no guard, so a
  stuck bus before the tick would have spun forever. Both now refuse with
  `kInvalidState`. Unreachable today -- both are called only after
  `Board::Init()` -- and reachable the moment anything talks to a device during
  bring-up, which is exactly what stage 2 invites.
- Rejected, for now, an init-level registry in the style of Zephyr's
  `PRE_KERNEL_1`/`POST_KERNEL`. With one board and no off-chip devices it would
  be a framework guarding a state that has not occurred. Its trigger is written
  down instead: two off-chip devices with an ordering constraint between them.
- Deferred a reset-valid timebase (`Micros()` on `DWT->CYCCNT`), which would
  make timeouts work in every stage and dissolve the boundary entirely. Worth
  doing when a stage-1 peripheral needs a bounded wait, and not before.
- Known gaps recorded rather than solved: `common::Error` has no payload, so
  `Board::Init()` cannot name which device failed; there is nowhere to report a
  bring-up failure to, since the console is a stage-3 resource; and an RTOS
  will want `SysTick` for itself.

### 2026-09-06: Peripheral configuration moved into the constructor (#37, #38)

- `mcu::GpioPin` and `mcu::I2CBus` now configure the hardware as they are
  constructed. `configured_`/`initialized_` and the eight `kInvalidState`
  guards they gated are gone, and `NucleoF767ZiBoard`'s member list is now a
  description of the board rather than a set of promises `Init()` has to keep.
  `Init()` is left with `InitSysTick()`, which needs the NVIC and so genuinely
  cannot run before `main()`.
- #37 assumed a constructor could not touch registers, because the board is a
  namespace-scope object. That turned out not to hold. `Reset_Handler` copies
  `.data`, zeroes `.bss` and calls `SystemInit` *before* `__libc_init_array`;
  `SystemInit` only enables the FPU and sets VTOR; no code here configures
  clocks at all (the F767 runs on HSI at 16 MHz out of reset, which every
  timing constant already assumes); and RCC is live from reset, with each
  driver enabling its own peripheral clock first. Two comments in the tree
  stated that ordering backwards and were corrected.
- Rejected the alternatives #37 listed. A factory with a private constructor
  (options 1/2) was the right answer *given* the no-registers-in-constructors
  rule, but once that rule proved unnecessary it was machinery guarding a state
  that no longer exists — and it forced `std::optional` members, which model a
  hardware absence that cannot happen on a fixed board. Option 3's friend
  declaration enforces who constructs, not who configures. Option 4's readiness
  gate became vacuous: declaring the member *is* the bring-up.
- Three invariants now hold this up, documented on `NucleoF767ZiBoard`:
  bring-up must not fail, must not call `mcu::Delay` (SysTick is not up yet),
  and this board must stay the only object with a dynamic initializer.
  `mcu::Usart` violates the first and stays two-phase, which also suits it:
  only the application knows its `UartConfig`.
- I2C bus speed became a constructor argument with a derived `TIMINGR` table
  (#38), deliberately undefaulted so a board has to name the rate its wiring
  can carry.
- `arm_cm7` gained its first test. Its public headers name no vendor type, so
  `test_peripheral_contract.cpp` compiles on the host whichever backend is
  selected and asserts the contract — that a pin cannot be constructed without
  a direction, and a bus without a speed. It proves the types, not the register
  writes; those still need the board in hand.

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
