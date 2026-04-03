/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "App/Client.hpp"

#include "App/EnhancedProtocol.hpp"
#include "Core/Request.hpp"
#include "Utils/Common.hpp"

#if defined(ESP32)
#include <lwip/sockets.h>
#elif defined(POSIX)
#include <sys/socket.h>
#include <unistd.h>
#endif

ebus::AbstractClient::AbstractClient(int fd, Request* request,
                                     bool writeCapable)
    : fd_(fd), request_(request), writeCapable_(writeCapable) {}

ebus::AbstractClient::~AbstractClient() { stop(); }

bool ebus::AbstractClient::isConnected() const { return fd_ >= 0; }

bool ebus::AbstractClient::available() {
  uint8_t dummy;
  return recv(fd_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT) > 0;
}

void ebus::AbstractClient::stop() {
  if (fd_ >= 0) {
#if defined(ESP32)
    lwip_close(fd_);
#else
    close(fd_);
#endif
    fd_ = -1;
  }
}

ebus::ReadOnlyClient::ReadOnlyClient(int fd, Request* request)
    : AbstractClient(fd, request, false) {}

bool ebus::ReadOnlyClient::available() { return false; }

bool ebus::ReadOnlyClient::recvFromClient(uint8_t&) { return false; }

void ebus::ReadOnlyClient::sendToClient(const std::vector<uint8_t>& data) {
  if (fd_ < 0 || data.empty()) return;
  if (send(fd_, data.data(), data.size(), MSG_DONTWAIT) < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      stop();
    }
  }
}

ebus::Action ebus::ReadOnlyClient::onBusByte(uint8_t) { return Action::Stop; }

ebus::RegularClient::RegularClient(int fd, Request* request)
    : AbstractClient(fd, request, true) {}

bool ebus::RegularClient::available() {
  uint8_t dummy;
  return recv(fd_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT) > 0;
}

bool ebus::RegularClient::recvFromClient(uint8_t& out) {
  return recv(fd_, &out, 1, MSG_DONTWAIT) == 1;
}

void ebus::RegularClient::sendToClient(const std::vector<uint8_t>& data) {
  if (fd_ < 0 || data.empty()) return;
  if (send(fd_, data.data(), data.size(), MSG_DONTWAIT) < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      stop();
    }
  }
}

ebus::Action ebus::RegularClient::onBusByte(uint8_t byte) {
  // Handle bus response according to last command
  switch (request_->getResult()) {
    case RequestResult::observeSyn:
    case RequestResult::firstLost:
    case RequestResult::firstError:
    case RequestResult::retryError:
    case RequestResult::secondLost:
    case RequestResult::secondError:
      return Action::Stop;
    case RequestResult::observeData:
      sendToClient({byte});
      return Action::Continue;
    case RequestResult::firstSyn:
    case RequestResult::firstRetry:
    case RequestResult::retrySyn:
      // Hide micro-retry: session remains active but we send no bridge response
      return Action::Continue;
    case RequestResult::firstWon:
    case RequestResult::secondWon:
      sendToClient({byte});
      return Action::Continue;
    default:
      break;
  }
  return Action::Stop;
}

ebus::EnhancedClient::EnhancedClient(int fd, Request* request)
    : AbstractClient(fd, request, true) {
  std::string ver = "ebus-service 1.0\n";
  send(fd_, ver.c_str(), ver.length(), 0);
}

bool ebus::EnhancedClient::available() {
  uint8_t dummy;
  return recv(fd_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT) > 0;
}

bool ebus::EnhancedClient::recvFromClient(uint8_t& out) {
  uint8_t b1;
  if (recv(fd_, &b1, 1, MSG_PEEK | MSG_DONTWAIT) != 1) return false;

  if (b1 < 0x80) {
    return recv(fd_, &out, 1, MSG_DONTWAIT) == 1;
  }

  uint8_t buf[2];
  if (recv(fd_, buf, 2, MSG_PEEK | MSG_DONTWAIT) != 2) return false;
  recv(fd_, buf, 2, MSG_DONTWAIT);

  if (!enhanced::Protocol::isValidSequence(buf[0], buf[1])) {
    sendToClient({enhanced::RESP_ERROR_HOST, enhanced::ERR_FRAMING});
    stop();
    return false;
  }

  uint8_t cmd, data;
  enhanced::Protocol::decode(buf, cmd, data);

  switch (cmd) {
    case enhanced::CMD_INIT:
      sendToClient({enhanced::RESP_RESETTED, 0x0});
      return false;
    case enhanced::CMD_SEND:
      out = data;
      return true;
    case enhanced::CMD_START:
      //   if (data == ebus::sym_syn) return false;
      out = data;
      return true;
    case enhanced::CMD_INFO:
      return false;
    default:
      break;
  }
  return false;
}

void ebus::EnhancedClient::sendToClient(const std::vector<uint8_t>& data) {
  if (fd_ < 0 || data.empty()) return;

  uint8_t cmd;
  uint8_t val;

  if (data.size() == 1) {
    // Single byte from broadcast loop: always a received notification
    cmd = enhanced::RESP_RECEIVED;
    val = data[0];
  } else {
    cmd = data[0];
    val = data[1];
  }

  // Short form is allowed for RESP_RECEIVED notifications where value < 0x80
  if (cmd == enhanced::RESP_RECEIVED && val < 0x80) {
    if (send(fd_, &val, 1, MSG_DONTWAIT) < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) stop();
    }
  } else {
    uint8_t out[2];
    enhanced::Protocol::encode(cmd, val, out);
    if (send(fd_, out, 2, MSG_DONTWAIT) < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) stop();
    }
  }
}

ebus::Action ebus::EnhancedClient::onBusByte(uint8_t byte) {
  // Handle bus response according to last command
  switch (request_->getResult()) {
    case RequestResult::firstLost:
    case RequestResult::secondLost:
      sendToClient({enhanced::RESP_FAILED, byte});
      return Action::Stop;
    case RequestResult::firstError:
    case RequestResult::retryError:
    case RequestResult::secondError:
      sendToClient({enhanced::RESP_ERROR_EBUS, enhanced::ERR_FRAMING});
      return Action::Stop;
    case RequestResult::observeSyn:
    case RequestResult::observeData:
      sendToClient({enhanced::RESP_RECEIVED, byte});
      return Action::Continue;
    case RequestResult::firstSyn:
    case RequestResult::firstRetry:
    case RequestResult::retrySyn:
      // Hide micro-retry: session remains active but we send no bridge response
      return Action::Continue;
    case RequestResult::firstWon:
    case RequestResult::secondWon:
      sendToClient({enhanced::RESP_STARTED, byte});
      return Action::Continue;
    default:
      break;
  }
  return Action::Stop;
}

std::unique_ptr<ebus::AbstractClient> ebus::createClient(int fd, Request* req,
                                                         ClientType type) {
  switch (type) {
    case ClientType::ReadOnly:
      return std::unique_ptr<AbstractClient>(new ReadOnlyClient(fd, req));
    case ClientType::Regular:
      return std::unique_ptr<AbstractClient>(new RegularClient(fd, req));
    case ClientType::Enhanced:
      return std::unique_ptr<AbstractClient>(new EnhancedClient(fd, req));
    default:
      return nullptr;
  }
}