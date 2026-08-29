# host-emulator

A Python hardware emulator for the host (software-in-the-loop) build. C++
applications talk to it over two ZeroMQ PAIR sockets carrying JSON messages;
this package plays the "hardware" side: pins that can be read and driven,
a loopback UART, and an I2C bus with per-address device buffers.

## Running

```bash
# Terminal 1: the emulator (binds its endpoint, then serves until Ctrl-C)
uv run host-emulator

# Terminal 2: any host-built app
../../build/host/bin/Debug/blinky
```

The integration tests under `tests/` manage both processes themselves; run
them via CTest (`ctest --preset=host-debug -R host_emulator_test`), which
supplies the app paths.

## Transport

Two ipc:// endpoints, one per direction, both ZMQ PAIR:

| Endpoint (default)                 | Bound by | Carries                          |
| ---------------------------------- | -------- | -------------------------------- |
| `ipc:///tmp/device_emulator.ipc`   | emulator | device → emulator requests       |
| `ipc:///tmp/emulator_device.ipc`   | device   | emulator → device requests       |

Each side replies on the socket it received the request from, so every
exchange is a strict request/response pair. Endpoint ownership is guarded by
`endpoint.py` (an flock-based lock plus a connect() liveness probe), the
Python counterpart of the C++ `EndpointLock` — see that module's docstring.

## Wire protocol

Every message is one JSON object. The vocabulary is defined on the C++ side
in `src/libs/mcu/host/emulator_message_json_encoder.hpp` and mirrored here by
the `StrEnum`s in `common.py`; **the two must change together**.

Envelope fields, present in every message:

| Field    | Values                        | Meaning                          |
| -------- | ----------------------------- | -------------------------------- |
| `type`   | `Request`, `Response`         | initiates vs. answers an exchange |
| `object` | `Pin`, `Uart`, `I2C`          | which peripheral kind is addressed |
| `name`   | e.g. `"LED 1"`, `"UART 1"`    | which instance                   |

Per-object operations and their extra fields:

### Pin

| Operation | Direction        | Request fields | Response fields    |
| --------- | ---------------- | -------------- | ------------------ |
| `Set`     | either direction | `state`        | `state`, `status`  |
| `Get`     | either direction | `state` (ignored) | `state`, `status` |

`state` is `Low`, `High`, or `Hi_Z`. The device may only `Set` its output
pins; the emulator drives input pins (e.g. pressing `Button 1`), which is how
pin interrupts are exercised.

### Uart

| Operation | Direction         | Request fields                | Response fields                        |
| --------- | ----------------- | ----------------------------- | -------------------------------------- |
| `Send`    | device → emulator | `data`                        | `bytes_transferred`, `status`          |
| `Receive` | device → emulator | `size`, `timeout_ms`          | `data`, `bytes_transferred`, `status`  |
| `Receive` | emulator → device | `data`, `size`                | `bytes_transferred`, `status`          |

`data` is an array of byte values. An emulator-initiated `Receive` pushes
unsolicited data at the device, which delivers it to the app's RxHandler and
acks.

### I2C

| Operation | Direction         | Request fields            | Response fields                                  |
| --------- | ----------------- | ------------------------- | ------------------------------------------------ |
| `Send`    | device → emulator | `address`, `data`         | `address`, `bytes_transferred`, `status`         |
| `Receive` | device → emulator | `address`, `size`         | `address`, `data`, `bytes_transferred`, `status` |

The emulator keeps one buffer per `address`; `write_to_device()` lets a test
pre-load one.

`status` values mirror C++ `common::Error` (`Ok`, `InvalidArgument`,
`InvalidOperation`, `Timeout`, ...); see `common.py` for the full set.

## Package layout

- `emulator.py` — `DeviceEmulator`: sockets, bind/serve lifecycle, routing
- `peripheral.py` — shared `Peripheral` behavior (routing, hooks, `_wait_for`)
- `pin.py`, `uart.py`, `i2c.py` — the per-peripheral protocols
- `common.py` — the mirrored wire vocabulary
- `endpoint.py` — endpoint ownership guards

## Tooling

uv for environments, ruff for lint/format, strict mypy over `src` and
`tests`; all configured in `pyproject.toml`.
