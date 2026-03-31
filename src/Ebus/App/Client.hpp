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

namespace ebus {

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

  virtual bool available();
  virtual bool readByte(uint8_t& out);
  virtual void writeBytes(const std::vector<uint8_t>& data);

  // Logic to determine if the client wants to continue sending after a byte
  virtual bool handleBusData(uint8_t byte) = 0;

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
  bool readByte(uint8_t& out) override;
  bool handleBusData(uint8_t byte) override;
};

/**
 * Regular Client: A simple byte-for-byte bridge. No special protocol, just
 * forwards bytes between the bus and the client.
 */
class RegularClient : public AbstractClient {
 public:
  RegularClient(int fd, Request* request);

  bool handleBusData(uint8_t byte) override;
};

/**
 * Enhanced Client: Implements the ebusd binary protocol.
 * Supports 2-byte escaped commands and encoded response status.
 */
class EnhancedClient : public AbstractClient {
 public:
  EnhancedClient(int fd, Request* request);

  bool readByte(uint8_t& out) override;
  void writeBytes(const std::vector<uint8_t>& data) override;

  bool handleBusData(uint8_t byte) override;
};

std::unique_ptr<AbstractClient> createClient(int fd, Request* req,
                                             ClientType type);

}  // namespace ebus