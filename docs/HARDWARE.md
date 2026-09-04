# Hardware: STM32F767ZI Nucleo-144

The first physical target. Everything below assumes the board's ST-LINK USB
connector (CN1), which provides power, flashing and a virtual COM port over one
cable.

## Build

```bash
cmake --workflow --preset=nucleo-f767zi-debug
cmake --workflow --preset=nucleo-f767zi-release
```

Both produce `.elf`, `.bin` and `.hex` under
`build/nucleo-f767zi/bin/<config>/`, plus a `.map` beside the build tree.

Configure and build only: firmware has no tests that run on the build machine.
What can be checked without a board is checked by:

```bash
tools/verify-firmware.sh build/nucleo-f767zi
```

which asserts the vector table sits at `0x08000000`, the entry point is
`Reset_Handler` with the Thumb bit set, `.init_array` is non-empty (so
namespace-scope constructors actually run), nothing is undefined, and the
interrupt-handler paths do not allocate. CI runs the same script.

## Flashing

**Flash from the host OS, not from the devcontainer.** Reaching an ST-LINK
from inside a container needs USB passthrough that is awkward on Linux and
effectively unavailable on macOS and Windows. Build in the container, flash
outside it — the build tree is on the shared checkout either way.

The board enumerates as a USB mass-storage volume named `NODE_F767ZI`. Copying
a raw binary onto it flashes the chip:

```bash
cp build/nucleo-f767zi/bin/Debug/blinky.bin /media/$USER/NODE_F767ZI/ && sync
```

The `sync` is not optional: without it the file can sit in the page cache and
nothing is written. If a copy appears to succeed but the board does not change
behaviour, update the ST-LINK firmware (STSW-LINK007) before suspecting the
code — older V2-1 firmware can accept the write and discard it.

Alternatives, both installed in the dev image:

```bash
st-info --probe   # expect chipid 0x451, dev-type STM32F76x_F77x
st-flash --reset write build/nucleo-f767zi/bin/Debug/blinky.bin 0x8000000
```

`--reset` is not optional. Without it `st-flash` writes and verifies happily,
reports "Go to Thumb mode", and leaves the core halted -- so a correct image
looks exactly like a broken one. The `0x8000000` is likewise mandatory: a raw
binary carries no load address. The `.hex` does, if you would rather not type
it:

```bash
st-flash --reset --format ihex write build/nucleo-f767zi/bin/Debug/blinky.hex
```

Or via OpenOCD, whose `reset` verb covers the same ground:

```bash
openocd -f board/st_nucleo_f7.cfg \
        -c "program build/nucleo-f767zi/bin/Debug/blinky.elf verify reset exit"
```

## Debugging

`gdb-multiarch` stands in for `arm-none-eabi-gdb`, which Ubuntu does not
package. Point your IDE's `gdbPath` at it.

```bash
openocd -f board/st_nucleo_f7.cfg                 # terminal 1
gdb-multiarch build/nucleo-f767zi/bin/Debug/blinky.elf
(gdb) target extended-remote :3333
(gdb) monitor reset halt
(gdb) load
```

## Pin map

The authority is `src/libs/board/stm32f767zi_nucleo/pin_map.hpp`; this table
is for reading. Source: UM1974, *STM32 Nucleo-144 boards*.

| Function | Pin | Notes |
|---|---|---|
| `UserLed1()` | PB0 | LD1, green |
| `UserLed2()` | PB7 | LD2, blue |
| — | PB14 | LD3, red; not yet exposed through `board::Board` |
| `UserButton1()` | PC13 | B1, blue. Externally pulled **down**: reads high while pressed, so a press is a rising edge |
| `Uart1()` | PD8 (TX), PD9 (RX) | USART3, AF7, wired to the ST-LINK virtual COM port — appears as `/dev/ttyACM0` |
| `I2C1()` | PB8 (SCL), PB9 (SDA) | AF4, Arduino D15/D14 |

## What works today

`blinky` is the end-to-end test: LD1 toggles every 200 ms, and pressing B1
lights LD2 from an interrupt handler (which only ever calls `SetHigh()`, so
LD2 latches on and stays lit).

To check the timing without a scope, count LD1's ON transitions over 10 s and
expect **25**. Each ON is a full ON→OFF→ON cycle, which is *two* toggles of
200 ms each — so 50 would mean the delay is running at half its intended
length, not that it is correct.

`uart_echo` is the second: it greets over the ST-LINK virtual COM port and
echoes what you type, toggling LD1 per byte received.

```bash
st-flash --reset write build/nucleo-f767zi/bin/Debug/uart_echo.bin 0x8000000
picocom -b 115200 --imap lfcrlf,crcrlf /dev/ttyACM0
```

Two things about reading that terminal, both of which look like faults and are
not:

- **Terminal emulators default to local echo off**, so what you type is not
  displayed. Every character on screen came from the board. Typing `hello` and
  seeing `hello` once means all five made the round trip — seeing it *twice*
  would mean something is echoing twice.
- **The greeting will often appear twice.** `st-flash --reset` boots the board
  and its greeting sits in the ST-LINK's USB buffer with nobody attached;
  opening the port then toggles DTR, resetting the board, so you get the
  buffered greeting and a fresh one.

`--imap` is worth the typing: the board sends bytes verbatim, so the greeting's
bare `\n` walks the cursor diagonally and an echoed Enter (a bare CR) makes
later output overwrite the same line.

`printf` and friends also reach this port: the board's `_write` (see
`syscalls.cpp`) retargets stdout and stderr to USART3, expanding `\n` to CRLF.
Output written before `Uart1().Init()` is discarded rather than blocking.

`I2C1()` is still a placeholder that returns `Error::kInvalidOperation`, so
`i2c_demo` links and runs but fails at its first peripheral call. See
`unimplemented_peripherals.hpp`.

### A note on allocation in interrupt handlers

`uart_echo`'s receive handler builds a `std::vector` — that is, it allocates,
in interrupt context. newlib-nano ships `__malloc_lock` as a no-op, so this
would re-enter the allocator whenever an interrupt arrived while the main
context was inside `malloc`, corrupting the arena in a way that surfaces as a
fault somewhere unrelated much later. `syscalls.cpp` overrides both lock
functions to mask interrupts around the allocator, which makes the pattern
safe here.

The handler also calls `Send()`, which busy-waits for the transmitter. At
115200 baud that is roughly 87 µs per byte spent inside an ISR. Fine for an
echo demo; not a pattern to copy into anything with latency requirements.

## Clock configuration

The chip runs from HSI at 16 MHz — its reset configuration, with no PLL, no
instruction or data cache, and no ART accelerator. This is deliberate. Each of
those is a change that can break peripherals that currently work (enabling the
D-cache in particular interacts with DMA buffers and with the DTCM/SRAM1
distinction), and each is worth making on its own against a baseline that is
known good.

`mcu::Delay` and `mcu::Millis` both assume 16 MHz. Raising the clock means
updating `kSystemCoreClockHz` in `systick.cpp` and `delay.cpp` together.

## Memory

512 KB of RAM as one contiguous region: DTCM 128 KB at `0x20000000`, SRAM1
368 KB at `0x20020000`, SRAM2 16 KB at `0x2007C000`. The linker script treats
them as one because they are address-contiguous. Split them if DMA ever needs
to avoid DTCM, which DMA can reach only through the CPU's AHBS slave port
(RM0410).

Flash is 2 MB at `0x08000000`. Current usage, Release: under 6 KB.
