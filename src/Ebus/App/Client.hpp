/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <ebus/Config.hpp>
#include <memory>
#include <string>
#include <vector>

#include "Core/Request.hpp"

namespace ebus {

/**
 * Action to be taken by the ClientManager after a byte is processed.
 */
enum class Action {
  Continue,  // Keep the client active
  Stop       // The session is over (success, failure, or end of telegram)
};

class Request;

/**
 * Abstract base for WiFi/Network clients (e.g. ebusd bridges).
 */
class AbstractClient {
 public:
  AbstractClient(int fd, Request* request, bool writeCapable);
  virtual ~AbstractClient();

  int getFd() const { return fd_; }
  bool isWriteCapable() const { return writeCapable_; }
  bool isConnected() const;

  virtual bool available() = 0;
  virtual bool recvFromClient(uint8_t& out) = 0;
  virtual void sendToClient(const std::vector<uint8_t>& data) = 0;

  // Logic to determine if the client wants to continue sending after a byte
  virtual Action onBusByte(const BusEventContext& ctx) = 0;

 protected:
  void stop();

  int fd_;
  Request* request_;
  bool writeCapable_;
};

/**
 * ReadOnly Client: Only reads from the bus, never writes back. Ideal for
 * monitoring or logging applications that don't need to interact with the bus.
 */
class ReadOnlyClient : public AbstractClient {
 public:
  ReadOnlyClient(int fd, Request* request);

  bool available() override;
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

  bool available() override;
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

  bool available() override;
  bool recvFromClient(uint8_t& out) override;
  void sendToClient(const std::vector<uint8_t>& data) override;

  Action onBusByte(const BusEventContext& ctx) override;
};

std::unique_ptr<AbstractClient> createClient(int fd, Request* req,
                                             ClientType type);

}  // namespace ebus