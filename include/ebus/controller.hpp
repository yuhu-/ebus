/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Public headers required for the API signatures
#include "ebus/callbacks.hpp"
#include "ebus/config.hpp"
#include "ebus/device.hpp"
#include "ebus/metrics.hpp"
#include "ebus/status.hpp"
#include "ebus/types.hpp"
#if defined(EBUS_SIMULATION)
#include "ebus/virtual_bus.hpp"
#endif

namespace ebus {

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
  Controller();
  explicit Controller(const EbusConfig& config);
  ~Controller();

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  /**
   * @brief Starts all internal service threads and the hardware interface.
   * @return true if the controller was started successfully.
   */
  bool start();

  /**
   * @brief Stops all background processing and releases hardware resources.
   */
  void stop();

  /**
   * @brief Checks if the controller is currently running.
   */
  bool isRunning() const noexcept;

  // --- Configuration Section ---

  /**
   * @brief Applies a complete configuration to the stack.
   * @param config The new configuration.
   * @return true if the configuration was valid and applied.
   * @note Some changes to BusConfig (like UART pins) may return false if called
   * while the controller is running.
   */
  bool configure(const EbusConfig& config);

  /**
   * @brief Checks if the controller has been configured.
   */
  bool isConfigured() const noexcept;

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
  static void setLogSink(
      std::function<void(LogLevel, const std::string&)> sink);

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

  // --- Callbacks Section ---

  /**
   * @brief Registers a callback for when this controller is addressed as a
   * slave.
   */
  void setReactiveMasterSlaveCallback(ReactiveMasterSlaveCallback callback);

  /**
   * @brief Registers a callback to receive all validated bus telegrams.
   */
  void setTelegramCallback(TelegramCallback callback);

  /**
   * @brief Registers a callback to receive protocol and hardware error
   * notifications.
   */
  void setErrorCallback(ErrorCallback callback);

  /**
   * @brief Registers a callback for low-level byte-by-byte protocol tracing.
   */
  void setTraceCallback(TraceCallback callback);

  // --- Runtime Section ---

  // Messaging & Scheduling

  /**
   * @brief Enqueues a message for transmission with a given priority.
   * @param priority Priority level (0-255, higher is more urgent).
   * @param message The raw message bytes.
   * @param callback Optional result notification.
   * @return true if the item was accepted into the queue.
   */
  bool enqueue(uint8_t priority, ByteView message,
               ResultCallback callback = nullptr);

  /**
   * @brief Enqueues a message to be sent at a specific time.
   */
  bool enqueueAt(uint8_t priority, ByteView message, Clock::time_point when,
                 ResultCallback callback = nullptr);

  // Polling

  /**
   * @brief Adds a recurring polling job.
   * @param priority Priority level.
   * @param message Message to send.
   * @param interval_ms Interval between polls.
   * @param callback Optional result notification.
   * @return A unique ID for the poll item, or 0 if rejected.
   */
  uint32_t addPollItem(uint8_t priority, ByteView message, uint32_t interval_ms,
                       ResultCallback callback = nullptr);

  /**
   * @brief Removes a recurring poll item by ID.
   */
  void removePollItem(uint32_t id);

  /**
   * @brief Clears all poll items.
   */
  void clearPollItems();

  // Device Discovery & Scanning

  /**
   * @brief Triggers an eBUS "Inquiry of Existence" broadcast.
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
   * @brief Checks if a discovery scan is currently in progress.
   */
  bool isScanning() const;

  // External Client Bridge (WiFi/TCP)

  /**
   * @brief Adds a network client bridge via an existing file descriptor.
   */
  void addClient(int fd, ClientType type);

  /**
   * @brief Disconnects and removes a network client.
   */
  void removeClient(int fd);

  // --- Diagnostics Section ---

  // Device Information

  /**
   * @brief Returns information about all discovered devices.
   */
  std::vector<DeviceInfo> getDeviceInfo() const;

  // Health Metrics

  /**
   * @brief Resets all hardware and protocol counters.
   */
  void resetMetrics();

  /**
   * @brief Returns a snapshot of the current system performance metrics.
   */
  ebus::Metrics getMetrics() const;

  /**
   * @brief Returns the recent history of bus utilization percentages.
   */
  std::vector<float> getUtilizationHistory() const;

  /**
   * @brief Returns the raw event trace of the last processed bytes.
   */
  std::vector<BusEventInfo> getTraceHistory() const;

  // Diagnostic Log

  /**
   * @brief Returns a snapshot of the diagnostic error log.
   */
  std::vector<ErrorEntry> getErrors() const;

  /**
   * @brief Returns the current capacity of the diagnostic error log.
   */
  size_t getErrorLogCapacity() const;

  /**
   * @brief Clears the diagnostic error log.
   */
  void clearErrors();

  /**
   * @brief Returns a minimal JSON string containing only thread stacks and
   * queue sizes.
   */
  std::string getSystemResourcesJson() const;

  /**
   * @brief Returns the service status as a JSON string.
   * @param reset_histories If true, resets history buffers after serialization.
   */
  std::string getServiceStatusJson(bool reset_histories = false) const;

#if defined(EBUS_SIMULATION)
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

  std::atomic<bool> configured_{false};
  std::atomic<bool> running_{false};

  mutable std::mutex config_mutex_;
  mutable std::mutex wake_mutex_;
  std::condition_variable wake_cv_;

  /**
   * @brief Internal helper to capture a snapshot of all service states.
   */
  ServiceStatus getServiceStatus() const;

  void processPublicEvents();

  void constructMembers();
  void run();
};

}  // namespace ebus