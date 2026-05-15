# C++ library for eBUS communication

This library enables communication with systems based on the eBUS protocol. eBUS is primarily used in the heating industry.

### eBUS Overview

*   **Baud Rate:** Fixed 2400 Baud.
*   **Physical:** Two-wire, Standard UART (8N1).
*   **Capacity:** Max 25 masters, 228 slaves.
*   **Protocol:** Byte-oriented arbitration and CRC-8 protection.

### Supported Platforms

*   **Linux / POSIX:** For gateway applications and diagnostics on PCs/Raspberry Pi.
*   **ESP32 / FreeRTOS:** For dedicated low-power hardware bridges (ESP-IDF).
*   **Virtual Bus:** Full protocol simulation mode for development without hardware (Build-time feature).

### Architecture and Development

The library is designed with a clear separation between the public API and internal protocol orchestration. For detailed information on the architectural patterns, component roles, and coding standards, please refer to the CONTRIBUTING.md guide.

### Diagnostics & Bus Health

The library includes a high-performance telemetry system accessible via `Controller::getMetrics()`. Metrics can be exported to JSON using `ebus::toJson(metrics)`. It provides:
- **Bus Utilization**: Real-time physical wire occupancy (compliant with eBUS Spec 2.2).
- **ebusd Compatibility:** Supports the Enhanced binary protocol for high-speed bridging.
- **Error Rate**: Percentage-based protocol health.
- **Contention Rate**: Collision monitoring during arbitration.
- **Jitter Analysis**: Timing statistics for SYN symbols and response latencies.

### Build Features

The library supports several build-time features that can be enabled or disabled via CMake options:

*   **EBUS_SIMULATION** (Default: OFF): Enables the virtual bus simulation infrastructure. This includes the `ebus::VirtualBus` API and allows running the library without physical hardware using an in-memory "Virtual Line". Transmission timing is simulated at the byte level to provide realistic protocol backpressure.

To enable simulation mode:
```bash
cmake -DEBUS_SIMULATION=ON ..
```

### Key Features
*   **Data Decoding**: Native support for 30+ eBUS data types including BCD, fixed-point (DATA2B/C), and float.
*   **Device Discovery**: Automatic identification of manufacturers and device roles. Includes specialized support for Vaillant service identification and serial number reconstruction.
*   **Zero-Allocation Path**: Core protocol FSM and byte stuffing utilize Small Buffer Optimization (SBO) to eliminate heap allocations during active bus communication.

### Scheduling and Priorities

The library features a priority-based `Scheduler`. Background tasks, such as the `DeviceScanner`, operate at a low priority (default 5). Applications can use the `Controller::enqueue` method with higher priority values (up to 255) to ensure critical messages preempt background traffic, guaranteeing minimal latency for user-initiated commands.

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

For reporting bugs and requesting features, please use the GitHub [Issues](https://github.com/yuhu-/ebus/issues) page.

---
*This project is licensed under the GPL-3.0-or-later.*
