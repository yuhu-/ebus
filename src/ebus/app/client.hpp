/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/config.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/request.hpp"

namespace ebus {

/**
 * Maximum size of the outbound buffer before a client is considered stalled.
 */
static constexpr size_t MAX_OUTBOUND_BUFFER_SIZE = 4096;

/**
 * Action to be taken by the ClientManager after a byte is processed.
 */
enum class Action {
  Continue,  // Keep the client active
  Stop       // The session is over (success, failure, or end of telegram),
             // ClientManager will remove this client.
};

class Request;

/**
 * Abstract base for WiFi/Network clients (e.g. ebusd bridges).
 */
class AbstractClient {
 public:
  AbstractClient(int fd, Request* request, bool write_capable);
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
   * Returns true if the client has data available to read from its socket.
   */
  virtual bool wantsToSend() = 0;
  virtual bool recvFromClient(uint8_t& out) = 0;
  virtual void sendToClient(const std::vector<uint8_t>& data) = 0;

  // Logic to determine if the client wants to continue sending after a byte
  virtual Action onBusByte(const BusEventContext& ctx) = 0;

 protected:
  int fd_;
  std::vector<uint8_t> outbound_buffer_;  // Per-client outbound buffer
  mutable std::mutex buffer_mutex_;       // Protects outboundBuffer_
  Request* request_;
  bool write_capable_;

  bool flushLocked();  // Internal flush logic; returns false if connection lost
};

/**
 * ReadOnly Client: Only reads from the bus, never writes back. Ideal for
 * monitoring or logging applications that don't need to interact with the bus.
 */
class ReadOnlyClient : public AbstractClient {
 public:
  ReadOnlyClient(int fd, Request* request);

  bool wantsToSend() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(const std::vector<uint8_t>& data) override;

  Action onBusByte(const BusEventContext& ctx) override;
};

/**
 * Regular Client: A simple byte-for-byte bridge. No special protocol, just
 * forwards bytes between the bus and the client.
 */
class RegularClient : public AbstractClient {
 public:
  RegularClient(int fd, Request* request);

  bool wantsToSend() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(const std::vector<uint8_t>& data) override;

  Action onBusByte(const BusEventContext& ctx) override;
};

/**
 * Enhanced Client: Implements the ebusd binary protocol.
 * Supports 2-byte escaped commands and encoded response status.
 */
class EnhancedClient : public AbstractClient {
 public:
  EnhancedClient(int fd, Request* request);

  bool wantsToSend() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(const std::vector<uint8_t>& data) override;

  Action onBusByte(const BusEventContext& ctx) override;

 private:
  std::vector<uint8_t> inbound_buffer_;
};

std::unique_ptr<AbstractClient> createClient(int fd, Request* req,
                                             ClientType type);

}  // namespace ebus