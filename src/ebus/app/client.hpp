/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/callbacks.hpp>
#include <ebus/config.hpp>
#include <ebus/types.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/enhanced_protocol.hpp"
#include "core/request.hpp"

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
  AbstractClient(int fd, Request* request, bool write_capable,
                 size_t max_buffer);
  virtual ~AbstractClient();

  void stop();

  /**
   * Attempts to flush the outbound buffer.
   * @return true if the client is still connected, false if the socket was
   * closed.
   */
  bool tryFlushOutboundBuffer();

  int getFd() const { return fd_; }
  bool isWriteCapable() const { return write_capable_; }
  bool isConnected() const { return fd_ >= 0; }
  bool hasPendingData() const { return !outbound_buffer_.empty(); }

  /**
   * Called when the manager starts a new active session for this client.
   */
  virtual void onSessionStart(uint32_t session_id) { (void)session_id; }

  /**
   * Returns true if the client has data available to read from its socket.
   */
  virtual bool wantsToSend() = 0;
  virtual bool recvFromClient(uint8_t& out) = 0;
  virtual void sendToClient(ByteView data) = 0;

  // Logic to determine if the client wants to continue sending after a byte
  virtual BridgeAction onBusByte(const BusEventInfo& info) = 0;

 protected:
  int fd_;
  std::vector<uint8_t> outbound_buffer_;  // Per-client outbound buffer
  mutable std::mutex buffer_mutex_;       // Protects outboundBuffer_
  Request* request_;
  size_t max_buffer_size_;
  bool write_capable_;
  uint8_t last_sent_byte_ = 0;

  bool flushLocked();  // Internal flush logic; returns false if connection lost
};

/**
 * ReadOnly Client: Only reads from the bus, never writes back. Ideal for
 * monitoring or logging applications that don't need to interact with the bus.
 */
class ReadOnlyClient : public AbstractClient {
 public:
  ReadOnlyClient(int fd, Request* request, size_t max_buffer);

  bool wantsToSend() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(ByteView data) override;

  BridgeAction onBusByte(const BusEventInfo& info) override;
};

/**
 * Regular Client: A simple byte-for-byte bridge. No special protocol, just
 * forwards bytes between the bus and the client.
 */
class RegularClient : public AbstractClient {
 public:
  RegularClient(int fd, Request* request, size_t max_buffer);

  bool wantsToSend() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(ByteView data) override;

  BridgeAction onBusByte(const BusEventInfo& info) override;
};

/**
 * Enhanced Client: Implements the ebusd binary protocol.
 * Supports 2-byte escaped commands and encoded response status.
 */
class EnhancedClient : public AbstractClient {
 public:
  EnhancedClient(int fd, Request* request, size_t max_buffer);

  bool wantsToSend() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(ByteView data) override;

  void onSessionStart(uint32_t session_id) override;

  BridgeAction onBusByte(const BusEventInfo& info) override;

 private:
  // The Enhanced protocol accumulation buffer (max 2 bytes for escaped
  // sequences)
  uint8_t inbound_buf_[2];
  size_t inbound_len_ = 0;

  void sendEnhancedResponse(enhanced::Response res, uint8_t val);
};

std::unique_ptr<AbstractClient> createClient(int fd, Request* req,
                                             ClientType type,
                                             size_t max_buffer);

}  // namespace ebus::detail