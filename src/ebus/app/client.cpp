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
  if (outbound_buffer_.empty()) return true;

  size_t total_sent = 0;
  const size_t to_send = outbound_buffer_.size();
  while (total_sent < to_send) {
    ssize_t n =
        platform::send(fd_, outbound_buffer_.data() + total_sent,
                       to_send - total_sent, platform::Flags::dont_wait);
    if (n > 0) {
      total_sent += static_cast<size_t>(n);
    } else if (n < 0) {
      if (platform::isInterrupted()) continue;
      if (platform::isWouldBlock()) break;
      stop();
      return false;
    } else {
      stop();
      return false;
    }
  }

  if (total_sent > 0) {
    outbound_buffer_.erase(outbound_buffer_.begin(),
                           outbound_buffer_.begin() + total_sent);
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
    // DRAIN: Discard oldest data if buffer is full to prevent
    // backpressure/hangs.
    while (outbound_buffer_.size() + data.size() > max_buffer_size_) {
      // Drop a substantial chunk (approx 12.5%) to amortize erase costs.
      size_t to_drop =
          std::max(data.size(), max_buffer_size_ /
                                    NetworkLimits::outbound_buffer_drop_factor);
      if (to_drop > outbound_buffer_.size()) to_drop = outbound_buffer_.size();
      outbound_buffer_.erase(outbound_buffer_.begin(),
                             outbound_buffer_.begin() + to_drop);
    }

    outbound_buffer_.insert(outbound_buffer_.end(), data.begin(), data.end());
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
  return ClientInfo{fd_, "read_only", isConnected(), write_capable_,
                    outbound_buffer_.size()};
}

RegularClient::RegularClient(int fd, Request* request, size_t max_buffer)
    : AbstractClient(fd, request, true, max_buffer) {}

bool RegularClient::wantsToSend() {
  uint8_t dummy;
  return platform::recv(fd_, &dummy, 1, platform::Flags::peek) > 0;
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
    // DRAIN: Discard oldest data if buffer is full to prevent
    // backpressure/hangs.
    while (outbound_buffer_.size() + data.size() > max_buffer_size_) {
      // Drop a substantial chunk (approx 12.5%) to amortize erase costs.
      size_t to_drop =
          std::max(data.size(), max_buffer_size_ /
                                    NetworkLimits::outbound_buffer_drop_factor);
      if (to_drop > outbound_buffer_.size()) to_drop = outbound_buffer_.size();
      outbound_buffer_.erase(outbound_buffer_.begin(),
                             outbound_buffer_.begin() + to_drop);
    }

    outbound_buffer_.insert(outbound_buffer_.end(), data.begin(), data.end());
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
  return ClientInfo{fd_, "regular", isConnected(), write_capable_,
                    outbound_buffer_.size()};
}

EnhancedClient::EnhancedClient(int fd, Request* request, size_t max_buffer)
    : AbstractClient(fd, request, true, max_buffer) {}

void EnhancedClient::onSessionStart(uint32_t session_id) {
  (void)session_id;
  inbound_len_ = 0;
}

bool EnhancedClient::wantsToSend() {
  if (inbound_len_ > 0) return true;
  uint8_t dummy;
  return platform::recv(fd_, &dummy, 1, platform::Flags::peek) > 0;
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
    // DRAIN: Discard oldest data if buffer is full to prevent
    // backpressure/hangs.
    while (outbound_buffer_.size() +
               detail::EnhancedProtocolLimits::max_sequence_len >
           max_buffer_size_) {
      // Drop a substantial chunk (approx 12.5%) to amortize erase costs.
      size_t to_drop = std::max(
          EnhancedProtocolLimits::max_sequence_len,
          max_buffer_size_ / NetworkLimits::outbound_buffer_drop_factor);
      if (to_drop > outbound_buffer_.size()) to_drop = outbound_buffer_.size();
      outbound_buffer_.erase(outbound_buffer_.begin(),
                             outbound_buffer_.begin() + to_drop);
    }

    // Short form is allowed for RECEIVED notifications where value < 0x80
    if (cmd == static_cast<uint8_t>(enhanced::Response::received) &&
        val < detail::EnhancedProtocolLimits::data_threshold) {
      outbound_buffer_.push_back(val);
    } else {
      uint8_t out[detail::EnhancedProtocolLimits::max_sequence_len];
      enhanced::Protocol::encode(cmd, val, out);
      outbound_buffer_.insert(
          outbound_buffer_.end(), out,
          out + detail::EnhancedProtocolLimits::max_sequence_len);
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
  return ClientInfo{fd_, "enhanced", isConnected(), write_capable_,
                    outbound_buffer_.size()};
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