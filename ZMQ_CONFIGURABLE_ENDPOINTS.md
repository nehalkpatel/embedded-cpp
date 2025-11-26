# ZMQ Configurable Endpoints

**Branch**: `feature/socket-robustness`
**Date**: 2025-11-26
**Status**: ✅ Implemented

## Overview

Both the Python emulator and C++ HostBoard now support configurable ZMQ endpoints, enabling:
- **Parallel test execution** with unique IPC paths per instance
- **TCP transport** for remote debugging (`tcp://127.0.0.1:5555`)
- **Multiple emulator instances** for testing multi-device scenarios

## Python API

### DeviceEmulator

```python
from host_emulator import DeviceEmulator

# Default IPC endpoints
emulator = DeviceEmulator()
# from_device: "ipc:///tmp/device_emulator.ipc"
# to_device: "ipc:///tmp/emulator_device.ipc"

# Custom IPC endpoints (for parallel tests)
emulator = DeviceEmulator(
    from_device_endpoint="ipc:///tmp/test1_device_emulator.ipc",
    to_device_endpoint="ipc:///tmp/test1_emulator_device.ipc"
)

# TCP endpoints (for remote debugging)
emulator = DeviceEmulator(
    from_device_endpoint="tcp://127.0.0.1:5555",
    to_device_endpoint="tcp://127.0.0.1:5556"
)
```

### Parameters

- **`from_device_endpoint`** (str, optional): ZMQ endpoint to **bind** for receiving messages from the device.
  Default: `"ipc:///tmp/device_emulator.ipc"`

- **`to_device_endpoint`** (str, optional): ZMQ endpoint to **connect** for sending messages to the device.
  Default: `"ipc:///tmp/emulator_device.ipc"`

## C++ API

### HostBoard

```cpp
#include "libs/board/host/host_board.hpp"

// Default IPC endpoints
board::HostBoard board;
board.Init();

// Custom endpoints
board::HostBoard::Endpoints custom_endpoints{
    .to_emulator = "ipc:///tmp/test1_device_emulator.ipc",
    .from_emulator = "ipc:///tmp/test1_emulator_device.ipc"
};
board::HostBoard board(custom_endpoints);
board.Init();

// TCP endpoints
board::HostBoard::Endpoints tcp_endpoints{
    .to_emulator = "tcp://127.0.0.1:5555",
    .from_emulator = "tcp://127.0.0.1:5556"
};
board::HostBoard board(tcp_endpoints);
board.Init();
```

### Struct Definition

```cpp
struct Endpoints {
    std::string to_emulator{"ipc:///tmp/device_emulator.ipc"};
    std::string from_emulator{"ipc:///tmp/emulator_device.ipc"};
};
```

- **`to_emulator`**: ZMQ endpoint to **connect** for sending to emulator
- **`from_emulator`**: ZMQ endpoint to **bind** for receiving from emulator

## Usage Examples

### Parallel Testing

Run multiple test instances simultaneously without conflicts:

#### Python Test Fixture

```python
import pytest
import uuid
from host_emulator import DeviceEmulator

@pytest.fixture
def unique_emulator():
    """Create emulator with unique endpoints for parallel testing."""
    test_id = uuid.uuid4().hex[:8]
    emulator = DeviceEmulator(
        from_device_endpoint=f"ipc:///tmp/test_{test_id}_device_emulator.ipc",
        to_device_endpoint=f"ipc:///tmp/test_{test_id}_emulator_device.ipc"
    )
    emulator.start()
    yield emulator
    emulator.stop()
```

#### C++ Test

```cpp
TEST(ParallelTest, Instance1) {
    board::HostBoard::Endpoints endpoints{
        .to_emulator = "ipc:///tmp/parallel_test1_device_emulator.ipc",
        .from_emulator = "ipc:///tmp/parallel_test1_emulator_device.ipc"
    };
    board::HostBoard board(endpoints);
    ASSERT_TRUE(board.Init());
    // Test logic...
}

TEST(ParallelTest, Instance2) {
    board::HostBoard::Endpoints endpoints{
        .to_emulator = "ipc:///tmp/parallel_test2_device_emulator.ipc",
        .from_emulator = "ipc:///tmp/parallel_test2_emulator_device.ipc"
    };
    board::HostBoard board(endpoints);
    ASSERT_TRUE(board.Init());
    // Test logic...
}
```

### Remote Debugging

Debug C++ application on one machine, emulator on another:

#### Machine 1: Python Emulator

```python
# Listen on all interfaces
emulator = DeviceEmulator(
    from_device_endpoint="tcp://0.0.0.0:5555",
    to_device_endpoint="tcp://0.0.0.0:5556"
)
emulator.start()
```

#### Machine 2: C++ Application

```cpp
board::HostBoard::Endpoints remote_endpoints{
    .to_emulator = "tcp://192.168.1.100:5555",
    .from_emulator = "tcp://192.168.1.100:5556"
};
board::HostBoard board(remote_endpoints);
board.Init();
```

### Multi-Instance Testing

Test coordination between multiple virtual devices:

```python
# Emulator 1
emulator1 = DeviceEmulator(
    from_device_endpoint="ipc:///tmp/device1_from.ipc",
    to_device_endpoint="ipc:///tmp/device1_to.ipc"
)

# Emulator 2
emulator2 = DeviceEmulator(
    from_device_endpoint="ipc:///tmp/device2_from.ipc",
    to_device_endpoint="ipc:///tmp/device2_to.ipc"
)

emulator1.start()
emulator2.start()

# Test inter-device communication...
```

## Implementation Details

### Python Changes

**File**: [py/host-emulator/src/host_emulator/emulator.py](py/host-emulator/src/host_emulator/emulator.py)

1. Added class constants for default endpoints:
   ```python
   DEFAULT_FROM_DEVICE_ENDPOINT = "ipc:///tmp/device_emulator.ipc"
   DEFAULT_TO_DEVICE_ENDPOINT = "ipc:///tmp/emulator_device.ipc"
   ```

2. Updated `__init__()` to accept endpoint parameters:
   ```python
   def __init__(self, from_device_endpoint=None, to_device_endpoint=None):
   ```

3. Modified `run()` to use configured endpoint for binding
4. Modified `start()` to use configured endpoint for connecting
5. Fixed bug in `main()` where old context name was referenced

### C++ Changes

**Files**:
- [src/libs/board/host/host_board.hpp](src/libs/board/host/host_board.hpp)
- [src/libs/board/host/host_board.cpp](src/libs/board/host/host_board.cpp)

1. Added `Endpoints` struct with default values:
   ```cpp
   struct Endpoints {
       std::string to_emulator{"ipc:///tmp/device_emulator.ipc"};
       std::string from_emulator{"ipc:///tmp/emulator_device.ipc"};
   };
   ```

2. Added parameterized constructor:
   ```cpp
   explicit HostBoard(Endpoints endpoints);
   ```

3. Stored endpoints as member variable (declared first for proper initialization order)
4. Updated `Init()` to use `endpoints_.to_emulator` and `endpoints_.from_emulator`

## Testing

All existing tests pass with default endpoints:

```bash
$ cmake --build --preset=host --config Debug
$ ctest --preset=host -C Debug

100% tests passed, 0 tests failed out of 28
Total Test time (real) = 40.64 sec
```

## Socket Direction Reference

Understanding the direction is critical for proper configuration:

```
Python Emulator                          C++ Application
──────────────────                      ───────────────

from_device_socket                      to_emulator_socket
  BIND                                    CONNECT
  ipc:///tmp/device_emulator.ipc    ←─── ipc:///tmp/device_emulator.ipc
  (receives from device)                  (sends to emulator)

to_device_socket                        from_emulator_socket
  CONNECT                                 BIND
  ipc:///tmp/emulator_device.ipc    ───→ ipc:///tmp/emulator_device.ipc
  (sends to device)                       (receives from emulator)
```

**Key Points**:
- Emulator **binds** `from_device` (receives requests from C++ app)
- Emulator **connects** `to_device` (sends responses to C++ app)
- C++ app **connects** `to_emulator` (sends requests to emulator)
- C++ app **binds** `from_emulator` (receives responses from emulator)

## Transport Types

### IPC (Inter-Process Communication)
- **Format**: `ipc:///path/to/socket.ipc`
- **Use case**: Same machine, fast, Unix domain sockets
- **Note**: On Linux, creates file at path; clean up stale files automatically handled

### TCP
- **Format**: `tcp://hostname:port`
- **Examples**:
  - `tcp://127.0.0.1:5555` (localhost)
  - `tcp://0.0.0.0:5555` (bind all interfaces)
  - `tcp://192.168.1.100:5555` (specific IP)
- **Use case**: Network communication, remote debugging
- **Note**: Ensure firewall allows the ports

### Inproc (In-Process)
- **Format**: `inproc://identifier`
- **Use case**: Threads within same process
- **Note**: Requires shared ZMQ context (not currently supported across Python/C++)

## Benefits

✅ **Parallel Testing**: Run multiple test instances without socket conflicts
✅ **CI/CD Friendly**: Each test job can use unique endpoints
✅ **Remote Debugging**: Debug application remotely over network
✅ **Flexible Deployment**: Choose transport based on needs (IPC vs TCP)
✅ **Multi-Instance Support**: Test scenarios with multiple virtual devices
✅ **Backward Compatible**: Default behavior unchanged, existing code works as-is

## Migration Guide

### No Changes Needed

Existing code continues to work without modification:

```python
# Old code - still works
emulator = DeviceEmulator()
```

```cpp
// Old code - still works
board::HostBoard board;
board.Init();
```

### Optional Migration

To take advantage of configurable endpoints:

```python
# New code - custom endpoints
emulator = DeviceEmulator(
    from_device_endpoint="ipc:///tmp/my_test_from.ipc",
    to_device_endpoint="ipc:///tmp/my_test_to.ipc"
)
```

```cpp
// New code - custom endpoints
board::HostBoard::Endpoints endpoints{
    .to_emulator = "ipc:///tmp/my_test_from.ipc",
    .from_emulator = "ipc:///tmp/my_test_to.ipc"
};
board::HostBoard board(endpoints);
```

## Future Enhancements

Potential improvements for the future (not currently needed):

1. **Endpoint validation**: Check format before connecting/binding
2. **Auto-generate unique endpoints**: Helper function to create UUID-based paths
3. **Endpoint discovery**: Service discovery for multi-instance scenarios
4. **Inproc support**: Shared context for in-process threading

## Related Documentation

- [ZMQ_IMPROVEMENTS1.md](ZMQ_IMPROVEMENTS1.md) - Core transport improvements
- [ZMQ_SOCKET_IMPROVEMENTS.md](ZMQ_SOCKET_IMPROVEMENTS.md) - Socket setup best practices
- [CLAUDE.md](CLAUDE.md) - Project architecture and conventions

## References

- [ZeroMQ Transports](http://api.zeromq.org/master:zmq-ipc)
- [ZeroMQ TCP Transport](http://api.zeromq.org/master:zmq-tcp)
- [ZeroMQ PAIR Pattern](https://zguide.zeromq.org/docs/chapter2/#The-PAIR-Pattern)

---

**Implementation Date**: 2025-11-26
**Status**: ✅ Complete
**Backward Compatible**: Yes
