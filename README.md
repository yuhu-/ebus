# C++ library for eBUS communication

This library enables communication with systems based on the eBUS protocol. eBUS is primarily used in the heating industry.

### eBUS Overview

- The eBUS works on a two-wire bus with a speed of 2400 baud.
- Realisation with Standard UART with 8 bits + start bit + stop bit. 
- A maximum of 25 master and 228 slave participants are possible.
- The eBUS protocol is byte-oriented with byte-oriented arbitration.
- Data protection through 8-bit CRC.

### Class Overview

The library is designed with a clear separation between the public API and internal protocol orchestration.

#### Public API (`include/ebus/`)
- **Controller**: The primary interface for applications. It manages the lifecycle, scheduling, and diagnostic aggregation. Encapsulated using the PIMPL idiom to hide internal complexity.
- **Config**: Platform-independent configuration for the controller and hardware-specific bus settings.
- **Definitions**: Central source of truth for protocol symbols, enums, and callback signatures.
- **Metrics**: Unified data models for bus health monitoring (jitter, utilization, error rates).
- **Datatypes**: Advanced encoding/decoding utilities for eBUS-specific data formats (e.g., float-to-ebus conversion).

#### Internal Implementation (`src/Ebus/`)
- **App**: Orchestration layer containing the **Scheduler** (priority-based transmission), **PollManager** (recurring jobs), **ClientManager** (network bridging), **DeviceManager** (inventory), **DeviceScanner** (discovery), and **EnhancedProtocol** (advanced modes).
- **Core**: The protocol engine. **Handler** manages the Finite State Machine (FSM), **Telegram** encapsulates the frame structure/validation, **Sequence** handles multi-byte sequences, and **Request** handles byte-oriented arbitration.
- **Platform**: Abstraction layer for system interaction. Includes **Bus** (serial IO), **Queue** (data buffering), and **ServiceThread** (concurrency abstractions). POSIX and FreeRTOS are supported.

### Diagnostics & Bus Health

The library includes a unified telemetry system accessible via `Controller::getMetrics()`. It provides:
- **Bus Utilization**: Physical line low-time calculation.
- **Error Rate**: Percentage-based protocol health.
- **Contention Rate**: Collision monitoring during arbitration.
- **Jitter Analysis**: Timing statistics for SYN symbols and response latencies.

### Tools

**ebusread**: A diagnostic tool that interprets incoming streams as eBUS telegrams. Supports files, devices, pipes, and TCP sockets.

**playground**: A developer sandbox for testing library features and protocol edge cases.

### Building the Project

To build the library and the modern test suite:

```bash
mkdir build && cd build
cmake ..
make
```

### Running Tests

Modern unit tests are built using [Catch2](https://github.com/catchorg/Catch2). You can run them using `ctest`:

```bash
cd build
ctest
```

### Legacy Development Tests

The project includes a collection of legacy tests used during development. These tests often provide detailed protocol traces and insights that are useful for debugging complex arbitration or timing issues.

These tests are excluded from the default build to keep build times fast. To enable them, use the `EBUS_BUILD_LEGACY_TESTS` CMake option:

```bash
cmake -DEBUS_BUILD_LEGACY_TESTS=ON ..
make
```

For reporting bugs and requesting features, please use the GitHub [Issues](https://github.com/yuhu-/ebus/issues) page.
