/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/status.hpp>
#include <ebus/types.hpp>
#include <memory>
#include <string>
#include <vector>

#include "app/enhanced_protocol.hpp"
#include "core/request.hpp"
#include "platform/mutex.hpp"
#include "platform/queue.hpp"
#include "platform/socket.hpp"

namespace ebus::detail {

class Request;

enum class BridgeAction {
  keep_active,   // Sniffing or heartbeat: stay in current wait state
  stop_session,  // Error or collision: drop client
  bypass_wait    // Wait phase complete (echo OK or arbitration won):
                 // move to transmit state for the next byte
};

/**
 * Abstract base for WiFi/Network clients (e.g. ebusd bridges).
 */
class AbstractClient {
 public:
  // Lifecycle
  AbstractClient(std::unique_ptr<platform::Socket> socket, Request* request,
                 bool write_capable, size_t max_buffer);
  virtual ~AbstractClient();
  void stop();

  // Special Members & Operators
  AbstractClient(const AbstractClient&) = delete;
  AbstractClient& operator=(const AbstractClient&) = delete;

  // Configuration
  int getFd() const { return socket_->getFd(); }
  bool isWriteCapable() const { return write_capable_; }

  // Working Methods
  /**
   * Attempts to flush the outbound buffer.
   * @return true if the client is still connected, false if the socket was
   * closed.
   */
  bool tryFlushOutboundBuffer();
  virtual void onSessionStart(uint32_t session_id) { (void)session_id; }
  virtual void processIncomingData(const uint8_t* data, size_t len) = 0;
  virtual bool hasPendingBusRequest() const = 0;
  virtual bool popPendingBusRequest(uint8_t& out) = 0;
  virtual void sendToClient(ByteView data) = 0;

  // Logic to determine if the client wants to continue sending after a byte
  virtual BridgeAction onBusByte(const BusEventInfo& info) = 0;

  // Status/Telemetry
  bool isConnected() const;
  bool hasPendingData() const { return count_ > 0; }
  virtual ClientInfo getClientInfo() const = 0;

  // ONE-SHOT SYN filter control
  void armSynFilter() { filter_next_syn_ = true; }

 protected:
  std::unique_ptr<platform::Socket> socket_;
  std::unique_ptr<uint8_t[]> buffer_storage_;  // Fixed-size circular storage
  size_t head_ = 0;                            // Index of the oldest byte
  size_t count_ = 0;                           // Current number of bytes stored
  mutable platform::Mutex buffer_mutex_;       // Protects outboundBuffer_
  Request* request_;
  size_t max_buffer_size_;
  bool write_capable_;
  uint8_t last_sent_byte_ = 0;
  bool filter_next_syn_ = false;  // ONE-SHOT: Filter next SYN (0xAA) only

  bool flushLocked();  // Internal flush logic; returns false if connection lost
};

/**
 * ReadOnly Client: Only reads from the bus, never writes back. Ideal for
 * monitoring or logging applications that don't need to interact with the bus.
 */
class ReadOnlyClient : public AbstractClient {
 public:
  // Lifecycle
  ReadOnlyClient(std::unique_ptr<platform::Socket> socket, Request* request,
                 size_t max_buffer);

  // Working Methods
  void processIncomingData(const uint8_t* data, size_t len) override;
  bool hasPendingBusRequest() const override;
  bool popPendingBusRequest(uint8_t& out) override;
  void sendToClient(ByteView data) override;

  BridgeAction onBusByte(const BusEventInfo& info) override;

  // Status/Telemetry
  ClientInfo getClientInfo() const override;
};

/**
 * Regular Client: A simple byte-for-byte bridge. No special protocol, just
 * forwards bytes between the bus and the client.
 */
class RegularClient : public AbstractClient {
 public:
  // Lifecycle
  RegularClient(std::unique_ptr<platform::Socket> socket, Request* request,
                size_t max_buffer);

  // Working Methods
  void processIncomingData(const uint8_t* data, size_t len) override;
  bool hasPendingBusRequest() const override;
  bool popPendingBusRequest(uint8_t& out) override;
  void sendToClient(ByteView data) override;

  BridgeAction onBusByte(const BusEventInfo& info) override;

  // Status/Telemetry
  ClientInfo getClientInfo() const override;

 private:
  platform::Queue<uint8_t> pending_bus_requests_;
};

/**
 * Enhanced Client: Implements the ebusd binary protocol.
 * Supports 2-byte escaped commands and encoded response status.
 */
class EnhancedClient : public AbstractClient {
 public:
  // Lifecycle
  EnhancedClient(std::unique_ptr<platform::Socket> socket, Request* request,
                 size_t max_buffer);
  void onSessionStart(uint32_t session_id) override;

  // Working Methods
  void processIncomingData(const uint8_t* data, size_t len) override;
  bool hasPendingBusRequest() const override;
  bool popPendingBusRequest(uint8_t& out) override;
  void sendToClient(ByteView data) override;

  BridgeAction onBusByte(const BusEventInfo& info) override;

  // Status/Telemetry
  ClientInfo getClientInfo() const override;

 private:
  // The Enhanced protocol accumulation buffer (max 2 bytes for escaped
  // sequences)
  uint8_t inbound_buf_[detail::EnhancedProtocolLimits::max_sequence_len];
  size_t inbound_len_ = 0;

  platform::Queue<uint8_t> pending_bus_requests_;

  void sendEnhancedResponse(enhanced::Response res, uint8_t val);
};

std::unique_ptr<AbstractClient> createClient(
    std::unique_ptr<platform::Socket> socket, Request* req, ClientType type,
    size_t max_buffer);

}  // namespace ebus::detail