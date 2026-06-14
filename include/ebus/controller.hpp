/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Public headers required for the API signatures
#include "ebus/callbacks.hpp"
#include "ebus/config.hpp"
#include "ebus/device.hpp"
#include "ebus/metrics.hpp"
#include "ebus/status.hpp"
#include "ebus/types.hpp"

namespace ebus {

class VirtualBus;
struct Impl;

/**
 * @brief The Controller is the primary entry point for using the eBUS library.
 *
 * It encapsulates the entire protocol stack, managing the physical bus
 * connection, message scheduling, background scanning, and telemetry. It
 * provides a thread-safe API for orchestration components.
 */
class Controller {
 public:
  // Lifecycle
  Controller();
  explicit Controller(const EbusConfig& config);
  ~Controller();
  bool start();
  void stop();

  // Special Members & Operators
  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  // Configuration

  /**
   * @brief Applies a complete configuration to the stack.
   * @param config The new configuration.
   * @return true if the configuration was valid and applied.
   * @note Some changes to BusConfig (like UART pins) may return false if called
   * while the controller is running.
   */
  bool configure(const EbusConfig& config);

  /**
   * @brief Applies a configuration update from a JSON string.
   * Validates the JSON schema and limits before applying.
   * @param json The configuration JSON (can be partial).
   * @return true if the configuration was valid and applied.
   */
  bool configure(std::string_view json);

  /**
   * @brief Returns the current configuration.
   */
  EbusConfig getConfig() const;

  /**
   * @brief Sets the controller's master address.
   */
  void setAddress(const uint8_t& address);

  /**
   * @brief Sets the maximum lock counter for arbitration (Spec 6.4).
   */
  void setLockCounter(const uint8_t& lock_counter);

  /**
   * @brief Enables or disables proactive sending of "Inquiry of Existence" on
   * startup.
   */
  void setSystemInquiry(bool enable);

  /**
   * @brief Enables or disables responding with a "Sign of Life" when an inquiry
   * is received.
   */
  void setSystemResponse(bool enable);

  /**
   * @brief Sets the timing window for arbitration in microseconds.
   */
  void setWindow(const uint16_t& window_us);

  /**
   * @brief Sets the hardware offset for UART context switching in microseconds.
   */
  void setOffset(const uint16_t& offset_us);

  /**
   * @brief Sets the inactivity timeout for the bus handler.
   */
  void setWatchdogTimeout(uint32_t timeout_ms);

  /**
   * @brief Sets the global log level for protocol events.
   */
  void setLogLevel(LogLevel level);

  /**
   * @brief Sets a callback for internal library log messages.
   */
  static void setLogSink(LogCallback sink);

  /**
   * @brief Sets the maximum number of errors to keep in the diagnostic log.
   */
  void setErrorLogSize(size_t size);

  /**
   * @brief Sets the session inactivity timeout for network clients.
   */
  void setSessionTimeout(uint32_t timeout_ms);

  /**
   * @brief Sets the inter-byte transmit timeout for active network bridge
   * streams.
   */
  void setTransmitTimeout(uint32_t timeout_ms);

  /**
   * @brief Sets the size of the outbound buffer for network clients.
   */
  void setOutboundBufferSize(size_t size);

  /**
   * @brief Enables or disables the automatic device scan on startup.
   */
  void setScanOnStartup(bool enable);

  /**
   * @brief Sets the maximum number of startup scan iterations.
   */
  void setMaxStartupScans(uint8_t max_scans);

  /**
   * @brief Sets the initial delay before the first scan starts.
   */
  void setInitialScanDelay(uint32_t delay_s);

  /**
   * @brief Sets the interval between autonomous background scans.
   */
  void setStartupScanInterval(uint32_t interval_s);

  /**
   * @brief Sets the maximum application-level retry attempts for a message.
   */
  void setMaxSendAttempts(uint8_t max_send_attempts);

  /**
   * @brief Sets the base duration for exponential backoff during retries.
   */
  void setBaseBackoff(uint32_t base_backoff_ms);

  /**
   * @brief Sets the internal state machine watchdog timeout.
   */
  void setFsmTimeout(uint32_t timeout_ms);

  /**
   * @brief Sets the global timeout for a full transfer attempt.
   */
  void setTotalTimeout(uint32_t timeout_ms);

  /**
   * @brief Registers a callback for when this controller is addressed as a
   * slave.
   */
  void setReactiveCallback(ReactiveCallback callback);

  /**
   * @brief Registers a unified callback for all protocol events (Telegrams or
   * Errors).
   */
  void setProtocolCallback(ProtocolCallback callback);

  /**
   * @brief Registers a callback for low-level byte-by-byte protocol tracing.
   */
  void setTraceCallback(TraceCallback callback);

  // Working Methods

  /**
   * @brief Enqueues a message for transmission with a given priority.
   * @param priority Priority level (0-255, higher is more urgent).
   * @param message The raw message bytes.
   * @return The session ID if accepted, 0 otherwise.
   */
  uint32_t enqueue(uint8_t priority, ByteView message);

  /**
   * @brief Enqueues a message to be sent at a specific time.
   */
  uint32_t enqueueAt(uint8_t priority, ByteView message,
                     Clock::time_point when);

  /**
   * @brief Adds a recurring polling job.
   * @param priority Priority level.
   * @param message Message to send.
   * @param interval_ms Interval between polls.
   * @return A unique ID for the poll item, or 0 if rejected.
   */
  uint32_t addPollItem(uint8_t priority, ByteView message,
                       uint32_t interval_ms);

  /**
   * @brief Removes a recurring poll item by ID.
   */
  void removePollItem(uint32_t id);

  /**
   * @brief Clears all poll items.
   */
  void clearPollItems();

  /**
   * @brief Standard eBUS System Discovery: Broadcast "Inquiry of Existence"
   * (07h FEh) This advises other masters that a new participant has entered the
   * bus.
   */
  void triggerInquiryOfExistence();

  /**
   * @brief Initiates an exhaustive scan of the entire bus address range.
   */
  void initFullScan(bool enable);

  /**
   * @brief Triggers a discovery scan for a specific slave address.
   */
  bool scanAddress(uint8_t address);

  /**
   * @brief Triggers discovery scans for a list of addresses.
   */
  bool scanAddresses(const std::vector<uint8_t>& addresses);

  /**
   * @brief Attempts to identify all currently observed (but unknown) devices.
   */
  bool scanObservedDevices();

  /**
   * @brief Adds a network client bridge via an existing file descriptor.
   */
  void addClient(int fd, ClientType type);
  void removeClient(int fd);

  // Status/Telemetry
  bool isRunning() const noexcept;
  bool isConfigured() const noexcept;
  bool isScanning() const;

  /**
   * @brief Returns information about all discovered devices.
   */
  void fetchDeviceInfo(std::function<void(const DeviceInfo&)> callback) const;

  /**
   * @brief Invokes a visitor callback with a snapshot of system performance
   * metrics.
   * @param callback Function to process the metrics reference.
   */
  void fetchMetrics(std::function<void(const Metrics&)> callback) const;

  /**
   * @brief Streams the system metrics JSON in chunks to the provided visitor.
   */
  void fetchMetrics(const JsonChunkVisitor& visitor, bool pretty = false) const;

  /**
   * @brief Returns the recent history of bus utilization percentages.
   */
  void fetchUtilizationHistory(std::function<void(float)> callback) const;

  /**
   * @brief Streams the bus utilization history JSON in chunks to the provided
   * visitor.
   */
  void fetchUtilizationHistory(const JsonChunkVisitor& visitor,
                               bool pretty = false) const;

  /**
   * @brief Returns the raw event trace of the last processed bytes.
   */
  void fetchTraceHistory(
      std::function<void(const BusEventInfo&)> callback) const;

  /**
   * @brief Streams the trace history JSON in chunks to the provided visitor.
   */
  void fetchTraceHistory(const JsonChunkVisitor& visitor,
                         bool pretty = false) const;

  /**
   * @brief Returns a snapshot of the diagnostic error log.
   */
  void fetchErrors(std::function<void(const ErrorEntry&)> callback) const;

  /**
   * @brief Streams the diagnostic error log JSON in chunks to the provided
   * visitor.
   */
  void fetchErrors(const JsonChunkVisitor& visitor, bool pretty = false) const;

  /**
   * @brief Returns the current capacity of the diagnostic error log.
   */
  size_t getErrorLogCapacity() const;

  /**
   * @brief Invokes a visitor callback with a snapshot of system resource usage.
   */
  void fetchStatus(std::function<void(const SystemResources&)> callback) const;

  /**
   * @brief Streams the service status JSON in chunks to the provided visitor.
   */
  void fetchStatus(const JsonChunkVisitor& visitor, bool pretty = false) const;

  /**
   * @brief Clears all historical data from the bus monitor (transitions,
   * utilization).
   */
  void clearHistories();

  /**
   * @brief Resets all hardware and protocol counters.
   */
  void resetMetrics();

  /**
   * @brief Clears the diagnostic error log.
   */
  void clearErrors();

#if EBUS_SIMULATION
  /**
   * @brief Returns a VirtualBus instance for direct interaction with the
   * simulated bus.
   * @note This method is only available when the library is built with
   * EBUS_SIMULATION.
   * @return A reference to the VirtualBus instance.
   */
  VirtualBus& getVirtualBus();
#endif

 private:
  EbusConfig config_;
  std::unique_ptr<Impl> impl_;

  friend struct Impl;
};

}  // namespace ebus