# C++ library for eBUS communication

This library enables communication with systems based on the eBUS protocol. eBUS is primarily used in the heating industry.

### eBUS Overview

- The eBUS works on a two-wire bus with a speed of 2400 baud.
- Realisation with Standard UART with 8 bits + start bit + stop bit. 
- A maximum of 25 master and 228 slave participants are possible.
- The eBUS protocol is byte-oriented with byte-oriented arbitration.
- Data protection through 8-bit CRC.

### Architecture and Development

The library is designed with a clear separation between the public API and internal protocol orchestration. For detailed information on the architectural patterns, component roles, and coding standards, please refer to the CONTRIBUTING.md guide.

### Diagnostics & Bus Health

The library includes a unified telemetry system accessible via `Controller::getMetrics()`. It provides:
- **Bus Utilization**: Physical line low-time calculation.
- **Error Rate**: Percentage-based protocol health.
- **Contention Rate**: Collision monitoring during arbitration.
- **Jitter Analysis**: Timing statistics for SYN symbols and response latencies.

### Tools

**ebusread**: A diagnostic tool that interprets incoming streams as eBUS telegrams. Supports files, devices, pipes, and TCP sockets.

**playground**: A developer sandbox for testing library features and protocol edge cases.

### Quick Start

To build the library and run the modern unit tests (Catch2):

```bash
mkdir build && cd build
cmake ..
make
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

---
*This project is licensed under the GPL-3.0-or-later.*
