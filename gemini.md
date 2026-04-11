# Gemini Context: eBUS Library

This document provides architectural context for Gemini Code Assist.

## Architectural Patterns
*   **PIMPL Idiom**: Used in `ebus::Controller` to provide a stable ABI and hide internal orchestration from the public API.
*   **FSM (Finite State Machine)**: The `Core::Handler` uses a state machine to manage the transition between synchronization, arbitration, data transfer, and ACK/NACK phases.
*   **HAL (Hardware Abstraction Layer)**: Hardware interaction is abstracted via `Platform::Bus`. Avoid using direct POSIX or FreeRTOS calls outside the `Platform/` directory.

## Critical Classes
*   **Controller**: The main entry point. Use this for lifecycle management.
*   **Config**: Centralized configuration for controller and bus settings.
*   **Bus**: Serial communication interface. Concrete implementations exist for POSIX and FreeRTOS.
*   **Telegram**: The unit of data. Handles CRC calculation and byte sequence validation.
*   **Sequence**: Manages timing and state for multi-byte protocol sequences.
*   **Handler**: The brain of the protocol. Manages byte-by-byte parsing.
*   **Request**: Manages state during the arbitration phase.
*   **Scheduler**: Manages master priority. eBUS is a multi-master protocol where masters compete for the bus using 2400 baud arbitration.
*   **PollManager**: Orchestrates periodic polling of slave devices.
*   **DeviceManager**: Maintains the inventory of discovered devices and their properties.
*   **DeviceScanner**: Implements the logic for scanning and discovering devices on the bus.
*   **EnhancedProtocol**: Implements specialized ebusd-compatible communication enhancements.
*   **ClientManager**: Manages network bridging and client connections.
*   **Metrics**: Unified data models for bus health and telemetry.
*   **Queue**: Thread-safe byte buffering for incoming bus data.
*   **ServiceThread**: The background worker thread that processes the **Queue** and drives the protocol FSM.

## Coding Standards
*   **C++14**: Strictly adhere to C++14 features. Do not use C++17 or later features (like `std::optional` or `std::string_view`) unless polyfilled.
*   **Threading**: Thread safety is required for the `Controller` API. Internal state updates must be synchronized as the `ServiceThread` processes the bus stream asynchronously.
*   **Memory**: Avoid heap allocation in the protocol processing loop. Utilize the pre-allocated `Queue` for bus byte buffering.
*   **Error Handling**: Use the `Metrics` system for protocol-level errors rather than exceptions in the hot path.

## Testing
*   **Catch2**: Preferred for unit testing.
*   **Legacy Tests**: Use these for timing-sensitive arbitration debugging.