/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client.hpp"

#include "app/enhanced_protocol.hpp"
#include "core/request.hpp"
#include "utils/common.hpp"

#if defined(ESP32)
#include <lwip/sockets.h>
#elif defined(POSIX)
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

ebus::AbstractClient::AbstractClient(int fd, Request* request,
                                     bool write_capable)
    : fd_(fd), request_(request), write_capable_(write_capable) {
  if (fd_ >= 0) {
    // All network clients must be non-blocking for the Manager's poll loop
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
  }
}

ebus::AbstractClient::~AbstractClient() { stop(); }

void ebus::AbstractClient::stop() {
  if (fd_ >= 0) {
    int type;
    socklen_t optlen = sizeof(type);
    if (::getsockopt(fd_, SOL_SOCKET, SO_TYPE, &type, &optlen) == 0) {
      ::shutdown(fd_, SHUT_RDWR);
    }
#if defined(ESP32)
    lwip_close(fd_);
#else
    ::close(fd_);
#endif
    fd_ = -1;
  }
}

bool ebus::AbstractClient::tryFlushOutboundBuffer() {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  return flushLocked();
}

bool ebus::AbstractClient::flushLocked() {
  if (fd_ < 0) return false;
  if (outbound_buffer_.empty()) return true;

  size_t totalSent = 0;
  const size_t toSend = outbound_buffer_.size();
  while (totalSent < toSend) {
    ssize_t n = ::send(fd_, outbound_buffer_.data() + totalSent,
                       toSend - totalSent, MSG_DONTWAIT);
    if (n > 0) {
      totalSent += static_cast<size_t>(n);
    } else if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      stop();
      return false;
    } else {
      stop();
      return false;
    }
  }

  if (totalSent > 0) {
    outbound_buffer_.erase(outbound_buffer_.begin(),
                           outbound_buffer_.begin() + totalSent);
  }
  return true;
}

ebus::ReadOnlyClient::ReadOnlyClient(int fd, Request* request)
    : AbstractClient(fd, request, false) {}

bool ebus::ReadOnlyClient::wantsToSend() { return false; }

bool ebus::ReadOnlyClient::recvFromClient(uint8_t& out) {
  (void)out;  // unused
  return false;
}

void ebus::ReadOnlyClient::sendToClient(const std::vector<uint8_t>& data) {
  if (fd_ < 0 || data.empty()) return;
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (outbound_buffer_.size() + data.size() > MAX_OUTBOUND_BUFFER_SIZE) {
      stop();
      return;
    }
    outbound_buffer_.insert(outbound_buffer_.end(), data.begin(), data.end());
    flushLocked();
  }
}

ebus::Action ebus::ReadOnlyClient::onBusByte(const BusEventContext& ctx) {
  if (!isConnected()) return Action::Stop;
  (void)ctx;
  return Action::Stop;
}

ebus::RegularClient::RegularClient(int fd, Request* request)
    : AbstractClient(fd, request, true) {}

bool ebus::RegularClient::wantsToSend() {
  uint8_t dummy;
  return ::recv(fd_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT) > 0;
}

bool ebus::RegularClient::recvFromClient(uint8_t& out) {
  return ::recv(fd_, &out, 1, MSG_DONTWAIT) == 1;
}

void ebus::RegularClient::sendToClient(const std::vector<uint8_t>& data) {
  if (fd_ < 0 || data.empty()) return;
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (outbound_buffer_.size() + data.size() > MAX_OUTBOUND_BUFFER_SIZE) {
      stop();
      return;
    }
    outbound_buffer_.insert(outbound_buffer_.end(), data.begin(), data.end());
    flushLocked();
  }
}

ebus::Action ebus::RegularClient::onBusByte(const BusEventContext& ctx) {
  if (!isConnected()) return Action::Stop;

  // Handle bus response according to last command
  switch (ctx.result) {
    case RequestResult::observe_syn:
    case RequestResult::first_lost:
    case RequestResult::first_error:
    case RequestResult::retry_error:
    case RequestResult::second_lost:
    case RequestResult::second_error:
      return Action::Stop;
    case RequestResult::observe_data:
      sendToClient({ctx.byte});
      return Action::Continue;
    case RequestResult::first_syn:
    case RequestResult::first_retry:
    case RequestResult::retry_syn:
      // Hide micro-retry: session remains active but we send no bridge response
      return Action::Continue;
    case RequestResult::first_won:
    case RequestResult::second_won:
      sendToClient({ctx.byte});
      return Action::Continue;
    default:
      break;
  }
  return Action::Stop;
}

ebus::EnhancedClient::EnhancedClient(int fd, Request* request)
    : AbstractClient(fd, request, true) {}

bool ebus::EnhancedClient::wantsToSend() {
  if (!inbound_buffer_.empty()) return true;
  uint8_t dummy;
  return ::recv(fd_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT) > 0;
}

bool ebus::EnhancedClient::recvFromClient(uint8_t& out) {
  // If we have an incomplete command in the buffer, try to finish it
  if (inbound_buffer_.empty()) {
    uint8_t b;
    if (::recv(fd_, &b, 1, MSG_DONTWAIT) != 1) return false;
    inbound_buffer_.push_back(b);
  }

  uint8_t b1 = inbound_buffer_[0];

  // Short form (< 0x80) is a single byte
  if (b1 < 0x80) {
    out = b1;
    inbound_buffer_.clear();
    return true;
  }

  // Enhanced sequences are always 2 bytes
  uint8_t buf[2];
  if (inbound_buffer_.size() < 2) {
    uint8_t b2;
    if (::recv(fd_, &b2, 1, MSG_DONTWAIT) != 1)
      return false;  // Still missing second byte
    inbound_buffer_.push_back(b2);
  }

  buf[0] = inbound_buffer_[0];
  buf[1] = inbound_buffer_[1];
  inbound_buffer_.clear();

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
      // Note: Arbitration cancellation via SYN is handled by Request FSM
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

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (outbound_buffer_.size() + 2 > MAX_OUTBOUND_BUFFER_SIZE) {
      stop();
      return;
    }

    // Short form is allowed for RESP_RECEIVED notifications where value < 0x80
    if (cmd == enhanced::RESP_RECEIVED && val < 0x80) {
      outbound_buffer_.push_back(val);
    } else {
      uint8_t out[2];
      enhanced::Protocol::encode(cmd, val, out);
      outbound_buffer_.insert(outbound_buffer_.end(), out, out + 2);
    }
    flushLocked();
  }
}

ebus::Action ebus::EnhancedClient::onBusByte(const BusEventContext& ctx) {
  if (!isConnected()) return Action::Stop;

  // Handle bus response according to last command
  switch (ctx.result) {
    case RequestResult::first_lost:
    case RequestResult::second_lost:
      sendToClient({enhanced::RESP_FAILED, ctx.byte});
      return Action::Stop;
    case RequestResult::first_error:
    case RequestResult::retry_error:
    case RequestResult::second_error:
      sendToClient({enhanced::RESP_ERROR_EBUS, enhanced::ERR_FRAMING});
      return Action::Stop;
    case RequestResult::observe_syn:
    case RequestResult::observe_data:
      sendToClient({enhanced::RESP_RECEIVED, ctx.byte});
      return Action::Continue;
    case RequestResult::first_syn:
    case RequestResult::first_retry:
    case RequestResult::retry_syn:
      // Hide micro-retry: session remains active but we send no bridge response
      return Action::Continue;
    case RequestResult::first_won:
    case RequestResult::second_won:
      sendToClient({enhanced::RESP_STARTED, ctx.byte});
      return Action::Continue;
    default:
      break;
  }
  return Action::Stop;
}

std::unique_ptr<ebus::AbstractClient> ebus::createClient(int fd, Request* req,
                                                         ClientType type) {
  switch (type) {
    case ClientType::read_only:
      return std::unique_ptr<AbstractClient>(new ReadOnlyClient(fd, req));
    case ClientType::regular:
      return std::unique_ptr<AbstractClient>(new RegularClient(fd, req));
    case ClientType::enhanced:
      return std::unique_ptr<AbstractClient>(new EnhancedClient(fd, req));
    default:
      return nullptr;
  }
}