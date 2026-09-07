# Boot Flow

How an application on this project gets from reset to running, and — the part
that matters when you add hardware — **which stage your new peripheral belongs
to and why**.

The short version: a peripheral's stage is decided by *what can fail*, not by
what is convenient.

## The sequence

Verified against the code rather than assumed; the ordering here is the sort of
thing that is easy to state backwards, and this repo has done so twice.

| # | Where | What happens |
|---|---|---|
| 1 | [`startup.s`](../src/libs/board/stm32f767zi_nucleo/startup.s) | `sp = _estack`, copy `.data` from flash, zero `.bss` |
| 2 | `bl SystemInit` → [`cortex_m7.cpp`](../src/libs/mcu/arm_cm7/cortex_m7.cpp) | Enable the FPU (`CPACR`), point `VTOR` at flash. **Core registers only.** |
| 3 | `bl __libc_init_array` | Static constructors run. `g_board` is built, and with it every on-chip peripheral. |
| 4 | `bl main` → [`main.cpp`](../src/libs/board/stm32f767zi_nucleo/main.cpp) | `app::AppMain(g_board)` → `RunApp<App>` |
| 5 | `Board::Init()` | `InitSysTick()`, then off-chip bring-up |
| 6 | `App::Init()` then `App::Run()` | Application. `Uart::Init` lives here. `Run()` never returns. |

Two consequences that are easy to get wrong:

- **`SystemInit` runs *after* `.data` and `.bss` are ready**, not before. It is
  restricted to core registers because that is all it needs, not because memory
  is unusable.
- **No code in this project configures clocks.** The F767 runs on HSI at 16 MHz
  out of reset and stays there. `RCC` is live from reset, and each driver
  enables its own peripheral clock as its first action — which is what makes
  step 3 able to touch registers at all.

## The stages

| Stage | Runs at | Nature | Can fail? | Timeouts work? |
|---|---|---|---|---|
| 0 — Core | step 2 | Core registers | No | No |
| 1 — On-chip | step 3 | Register writes, no dependencies | **No** | **No** |
| 2 — Off-chip | step 5 | I/O across a wire | **Yes, routinely** | **Yes** |
| 3 — Runtime | step 6 | Application, interrupts | Yes | Yes |

### Why "can it fail" is the dividing line

Bringing up an on-chip peripheral is a fixed sequence of register writes against
hardware that is soldered into the die. There is nothing to be absent, nothing
to time out, nothing to report. That is what lets it happen in a constructor —
and a constructor is worth having, because it means an unconfigured peripheral
cannot be reached. See the 2026-09-06 decision-log entry in
[`PROJECT_PLAN.md`](PROJECT_PLAN.md).

Bringing up an off-chip device is I/O. The device may be unpopulated, wrongly
strapped, held in reset, or simply slow. Failure is a *normal outcome*, so it
needs somewhere that can report one — which a constructor is not.

### The timebase is the hard boundary

`mcu::Millis()` does not advance until `InitSysTick()` runs in stage 2. Before
that:

- **`mcu::Delay` works.** It checks `SysTickRunning()` and falls back to a
  `DWT->CYCCNT` busy-wait ([`delay.cpp`](../src/libs/mcu/arm_cm7/delay.cpp)). An
  early delay is imprecise and burns cycles, but it is not a hang.
- **Bounded waits do not.** `(Millis() - start) > timeout` can never become true
  against a frozen counter, so a naive timeout loop spins forever. The drivers
  therefore *refuse* one this early — `I2CBus`'s flag wait and
  `Usart::Receive` with a non-zero timeout both return `kInvalidState` when the
  tick is not running, rather than hanging.

So a stage-1 constructor may wait, but it may not *talk to anything*, because
every real transfer needs a timeout. That is the whole reason off-chip bring-up
is stage 2.

## Where does my peripheral go?

```
Is it inside the MCU?
├─ Yes → Stage 1. A board member; its constructor configures it.
│        GPIO, I2C/SPI/UART controllers, ADC, timers.
│        Must not fail. Must not do I/O.
│
└─ No, it is across a wire → Stage 2. Brought up in Board::Init().
         Sensors, flash chips, radios, anything with its own part number.
         May fail, and should say so.
```

The ADC is the case that shows the split is not "sensor or not": the **ADC
peripheral** is stage 1, while the **sensor wired to it** is stage 2.

### Worked examples

| Device | Stage 1 (constructor) | Stage 2 (`Board::Init()`) |
|---|---|---|
| I2C temp/humidity sensor | `I2CBus` on its pins | Probe the device, read its ID, configure sampling |
| SPI NOR flash | `SpiBus` + a chip-select `GpioPin` | Read the JEDEC ID, verify capacity |
| Bluetooth module on UART | `Usart` on its pins, reset-line `GpioPin` | Release reset, wait, exchange a command, check the reply |
| ADC-based sensor | The ADC peripheral | Reference/calibration; the reading itself is stage 3 |

`mcu::Usart` is the exception in the table above and worth understanding: it is
on-chip but still two-phase, because only the *application* knows the
`UartConfig` it wants, and `Init()` validates that config and can fail. Being
on-chip makes a constructor *possible*, not mandatory.

## Known gaps

Named because they are load-bearing for what comes next, not because they block
anything today.

- **`Board::Init()` cannot say which device failed.** It returns
  `std::expected<void, common::Error>`, and `common::Error` carries no payload.
  With one off-chip device that is tolerable; with four it is not. This needs
  solving before stage 2 has real occupants.
- **There is nowhere to report a failure to.** `main.cpp` spins with the error
  in a register for a debugger to find, because the console is a stage-3
  resource brought up by the application. A board that fails bring-up cannot
  currently tell anyone.
- **An RTOS will want the tick.** FreeRTOS drives `SysTick` itself, and
  conventionally starts some drivers after the kernel does. Stage 3 is where
  that lands, and it will likely revisit step 5's ordering.
- **Stage 1 has no ordering mechanism between peripherals.** It is declaration
  order in the board class, which is well defined but implicit. Fine for
  independent peripherals; a problem the first time one depends on another.

## What would change this design

Deliberately written down so the next step is a decision rather than a drift.

- **A timebase valid from reset** — building `Micros()` on `DWT->CYCCNT`, which
  runs from reset and needs no interrupt — would make timeouts work in every
  stage and remove the hard boundary above. Worth doing when a stage-1
  peripheral genuinely needs a bounded wait. It is a timeout source, not an
  uptime clock: the counter wraps in ~268 s at 16 MHz.
- **Explicit init levels** (Zephyr's `PRE_KERNEL_1` / `POST_KERNEL`, or a
  registry with priorities) would replace the two-phase split. The trigger is
  concrete: **two off-chip devices with an ordering constraint between them.**
  Until then it is ceremony, and this project would rather show the seam than
  hide it behind a framework.
