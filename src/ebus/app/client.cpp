/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client.hpp"

#include <ebus/utils.hpp>

#include "platform/socket.hpp"

namespace ebus::detail {

AbstractClient::AbstractClient(int fd, Request* request, bool write_capable,
                               size_t max_buffer)
    : fd_(fd),
      buffer_storage_(new uint8_t[max_buffer]),
      request_(request),
      max_buffer_size_(max_buffer),
      write_capable_(write_capable) {
  if (fd_ >= 0) {
    // All network clients must be non-blocking for the Manager's poll loop
    platform::setNonBlocking(fd_);
  }
}

AbstractClient::~AbstractClient() { stop(); }

void AbstractClient::stop() {
  if (fd_ >= 0) {
    platform::close(fd_);
    fd_ = -1;
  }
}

bool AbstractClient::tryFlushOutboundBuffer() {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return flushLocked();
}

bool AbstractClient::flushLocked() {
  if (fd_ < 0) return false;
  if (count_ == 0) return true;

  while (count_ > 0) {
    // Send in one or two parts depending on circular wrap
    size_t part_len = std::min(count_, max_buffer_size_ - head_);
    ssize_t n = platform::send(fd_, &buffer_storage_[head_], part_len,
                               platform::Flags::dont_wait);

    if (n > 0) {
      head_ = (head_ + n) % max_buffer_size_;
      count_ -= n;
      // If we didn't send the full part, the socket buffer is likely full
      if (static_cast<size_t>(n) < part_len) break;
    } else if (n < 0) {
      if (platform::isInterrupted()) continue;
      if (platform::isWouldBlock()) break;
      stop();
      return false;
    }
  }

  return true;
}

ReadOnlyClient::ReadOnlyClient(int fd, Request* request, size_t max_buffer)
    : AbstractClient(fd, request, false, max_buffer) {}

bool ReadOnlyClient::wantsToSend() { return false; }

bool ReadOnlyClient::recvFromClient(uint8_t& out) {
  (void)out;  // unused
  return false;
}

void ReadOnlyClient::sendToClient(ByteView data) {
  if (fd_ < 0 || data.empty()) return;
  {
    platform::LockGuard<platform::Mutex> lock(buffer_mutex_);

    // Drop oldest data if we would exceed capacity (O(1) in circular buffer)
    if (count_ + data.size() > max_buffer_size_) {
      size_t to_drop = (count_ + data.size()) - max_buffer_size_;
      head_ = (head_ + to_drop) % max_buffer_size_;
      count_ -= to_drop;
    }

    for (uint8_t b : data) {
      buffer_storage_[(head_ + count_) % max_buffer_size_] = b;
      count_++;
    }
    flushLocked();
  }
}

BridgeAction ReadOnlyClient::onBusByte(const BusEventInfo&) {
  if (!isConnected()) return BridgeAction::stop_session;
  // This should never happen, because ReadOnlyClient is never allowed to write
  return BridgeAction::stop_session;
}

ClientInfo ReadOnlyClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return ClientInfo{fd_, "read_only", isConnected(), write_capable_, count_};
}

RegularClient::RegularClient(int fd, Request* request, size_t max_buffer)
    : AbstractClient(fd, request, true, max_buffer) {}

bool RegularClient::wantsToSend() {
  return platform::hasDataAvailable(fd_);
  // uint8_t dummy;
  // return platform::recv(fd_, &dummy, 1, platform::Flags::peek) > 0;
}

bool RegularClient::recvFromClient(uint8_t& out) {
  bool ret = platform::recv(fd_, &out, 1, platform::Flags::dont_wait) == 1;
  if (ret) last_sent_byte_ = out;
  return ret;
}

void RegularClient::sendToClient(ByteView data) {
  if (fd_ < 0 || data.empty()) return;
  {
    platform::LockGuard<platform::Mutex> lock(buffer_mutex_);

    if (count_ + data.size() > max_buffer_size_) {
      size_t to_drop = (count_ + data.size()) - max_buffer_size_;
      head_ = (head_ + to_drop) % max_buffer_size_;
      count_ -= to_drop;
    }

    for (uint8_t b : data) {
      buffer_storage_[(head_ + count_) % max_buffer_size_] = b;
      count_++;
    }
    flushLocked();
  }
}

BridgeAction RegularClient::onBusByte(const BusEventInfo& info) {
  if (!isConnected()) return BridgeAction::stop_session;

  switch (info.result) {
    case RequestResult::first_won:
    case RequestResult::second_won:
      // Arbitration won: send address echo back to client and proceed to data
      sendToClient(ByteView(&info.byte, 1));
      return BridgeAction::bypass_wait;

    case RequestResult::first_lost:
    case RequestResult::second_lost:
    case RequestResult::first_error:
    case RequestResult::second_error:
    case RequestResult::retry_error:
      // Fatal arbitration failure
      return BridgeAction::stop_session;

    case RequestResult::observe_data:
      // Echo verification: if we are active, the next data byte must match
      if (info.byte != last_sent_byte_) return BridgeAction::stop_session;
      sendToClient(ByteView(&info.byte, 1));
      return BridgeAction::bypass_wait;

    default:
      // Sniffing heartbeats (SYN) or transparent traffic
      sendToClient(ByteView(&info.byte, 1));
      return BridgeAction::keep_active;
  }
}

ClientInfo RegularClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return ClientInfo{fd_, "regular", isConnected(), write_capable_, count_};
}

EnhancedClient::EnhancedClient(int fd, Request* request, size_t max_buffer)
    : AbstractClient(fd, request, true, max_buffer) {}

void EnhancedClient::onSessionStart(uint32_t session_id) {
  (void)session_id;
  inbound_len_ = 0;
}

bool EnhancedClient::wantsToSend() {
  if (inbound_len_ > 0) return true;
  return platform::hasDataAvailable(fd_);
  // uint8_t dummy;
  // return platform::recv(fd_, &dummy, 1, platform::Flags::peek) > 0;
}

bool EnhancedClient::recvFromClient(uint8_t& out) {
  // If we have an incomplete command in the buffer, try to finish it
  if (inbound_len_ == 0) {
    uint8_t b;
    if (platform::recv(fd_, &b, 1, platform::Flags::dont_wait) != 1)
      return false;
    inbound_buf_[inbound_len_++] = b;
  }

  uint8_t b1 = inbound_buf_[0];

  // Short form (< 0x80) is a single byte
  if (b1 < detail::EnhancedProtocolLimits::data_threshold) {
    out = b1;
    inbound_len_ = 0;
    return true;
  }

  // Enhanced sequences are always 2 bytes
  if (inbound_len_ < detail::EnhancedProtocolLimits::max_sequence_len) {
    uint8_t b2;
    if (platform::recv(fd_, &b2, 1, platform::Flags::dont_wait) != 1)
      return false;
    inbound_buf_[inbound_len_++] = b2;
  }

  if (!enhanced::Protocol::isValidSequence(inbound_buf_[0], inbound_buf_[1])) {
    sendEnhancedResponse(enhanced::Response::error_host,
                         static_cast<uint8_t>(enhanced::Error::framing));
    inbound_len_ = 0;
    stop();
    return false;
  }

  enhanced::Command cmd;
  uint8_t data;
  enhanced::Protocol::decode(inbound_buf_, cmd, data);
  inbound_len_ = 0;

  switch (cmd) {
    case enhanced::Command::init:
      sendEnhancedResponse(enhanced::Response::resetted, 0x00);
      return false;
    case enhanced::Command::send:
      out = data;
      last_sent_byte_ = data;
      return true;
    case enhanced::Command::start:
      if (data == ebus::Symbols::syn) {
        request_->reset();
        return false;
      }
      out = data;
      last_sent_byte_ = data;
      return true;
    case enhanced::Command::info:
      return false;
    default:
      break;
  }

  return false;
}

void EnhancedClient::sendToClient(ByteView data) {
  if (fd_ < 0 || data.empty()) return;

  uint8_t cmd;
  uint8_t val;

  if (data.size() == 1) {
    // Single byte from broadcast loop: always a received notification
    cmd = static_cast<uint8_t>(enhanced::Response::received);
    val = data[0];
  } else {
    cmd = data[0];
    val = data[1];
  }

  {
    platform::LockGuard<platform::Mutex> lock(buffer_mutex_);

    constexpr size_t seq_len = detail::EnhancedProtocolLimits::max_sequence_len;
    auto pushByte = [this](uint8_t b) {
      if (count_ >= max_buffer_size_) {
        head_ = (head_ + 1) % max_buffer_size_;
        count_--;
      }
      buffer_storage_[(head_ + count_) % max_buffer_size_] = b;
      count_++;
    };

    // Ensure room for potential 2-byte sequence
    while (count_ + seq_len > max_buffer_size_) {
      head_ = (head_ + 1) % max_buffer_size_;
      count_--;
    }

    if (cmd == static_cast<uint8_t>(enhanced::Response::received) &&
        val < detail::EnhancedProtocolLimits::data_threshold) {
      pushByte(val);
    } else {
      uint8_t out[seq_len];
      enhanced::Protocol::encode(cmd, val, out);
      pushByte(out[0]);
      pushByte(out[1]);
    }
    flushLocked();
  }
}

BridgeAction EnhancedClient::onBusByte(const BusEventInfo& info) {
  if (!isConnected()) return BridgeAction::stop_session;

  switch (info.result) {
    case RequestResult::first_lost:
    case RequestResult::second_lost:
      // Arbitration lost: return 0x0A + the master address that actually won
      sendEnhancedResponse(enhanced::Response::failed, info.byte);
      return BridgeAction::stop_session;
    case RequestResult::first_error:
    case RequestResult::retry_error:
    case RequestResult::second_error:
      // Physical layer error
      sendEnhancedResponse(enhanced::Response::error_ebus,
                           static_cast<uint8_t>(enhanced::Error::framing));
      return BridgeAction::stop_session;
    case RequestResult::first_won:
    case RequestResult::second_won:
      // Arbitration won: signal started
      sendEnhancedResponse(enhanced::Response::started, info.byte);
      return BridgeAction::bypass_wait;
    case RequestResult::observe_data:
      // Verification vs Sniffing
      sendEnhancedResponse(enhanced::Response::received, info.byte);
      if (info.byte == last_sent_byte_) {
        return BridgeAction::bypass_wait;
      }
      return BridgeAction::keep_active;

    default:
      // Sniffing (SYN, retry steps, etc.)
      sendEnhancedResponse(enhanced::Response::received, info.byte);
      return BridgeAction::keep_active;
  }
  return BridgeAction::stop_session;
}

ClientInfo EnhancedClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return ClientInfo{fd_, "enhanced", isConnected(), write_capable_, count_};
}

void EnhancedClient::sendEnhancedResponse(enhanced::Response res, uint8_t val) {
  const uint8_t raw[2] = {static_cast<uint8_t>(res), val};
  sendToClient(ByteView(raw, 2));
}

std::unique_ptr<AbstractClient> createClient(int fd, Request* req,
                                             ClientType type,
                                             size_t max_buffer) {
  switch (type) {
    case ClientType::read_only:
      return std::unique_ptr<AbstractClient>(
          new ReadOnlyClient(fd, req, max_buffer));
    case ClientType::regular:
      return std::unique_ptr<AbstractClient>(
          new RegularClient(fd, req, max_buffer));
    case ClientType::enhanced:
      return std::unique_ptr<AbstractClient>(
          new EnhancedClient(fd, req, max_buffer));
    default:
      return nullptr;
  }
}

}  // namespace ebus::detail