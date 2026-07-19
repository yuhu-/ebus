/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app/client.hpp"

#include <ebus/utils.hpp>
#include <iostream>

namespace ebus::detail {

void show(const char* function, uint8_t cmd, uint8_t val, uint8_t out[2]) {
  std::cout << function << " cmd:val " << ebus::toString(cmd) << ":"
            << ebus::toString(val) << " " << ebus::toString(out[0]) << ":"
            << ebus::toString(out[1]) << std::endl;
}

AbstractClient::AbstractClient(std::unique_ptr<platform::Socket> socket,
                               Request* request, bool write_capable,
                               size_t max_buffer)
    : socket_(std::move(socket)),
      request_(request),
      write_capable_(write_capable),
      max_buffer_size_(max_buffer),
      outbound_buffer_(new uint8_t[max_buffer]) {}

AbstractClient::~AbstractClient() { stop(); }

void AbstractClient::stop() {
  if (socket_ && socket_->isValid()) {
    socket_->close();
  }
}

bool AbstractClient::flushOutgoingData() {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  return flushLocked();
}

bool AbstractClient::isConnected() const {
  return socket_ && socket_->isValid();
}

bool AbstractClient::flushLocked() {
  if (!socket_ || !socket_->isValid()) return false;
  if (count_ == 0) return true;

  while (count_ > 0) {
    // Send in one or two parts depending on circular wrap
    size_t part_len = std::min(count_, max_buffer_size_ - head_);
    ssize_t n = socket_->write(&outbound_buffer_[head_], part_len);

    if (n > 0) {
      head_ = (head_ + n) % max_buffer_size_;
      count_ -= static_cast<size_t>(n);
      // Partial send: kernel TCP send buffer full, stop for now (will retry)
      if (static_cast<size_t>(n) < part_len) break;
    } else if (n == 0) {
      // Shouldn't happen on a connected socket
      break;
    } else {
      // n < 0: check errno
      int err = errno;
      if (err == EAGAIN || err == EWOULDBLOCK || err == EINTR) {
        // Expected on non-blocking socket: buffer full or interrupted, try
        // later
        break;
      }
      // Real error (ECONNRESET, EPIPE, EBADF ...): close socket
      std::cout << "[Client fd=" << socket_->getFd()
                << "] write error errno=" << err << ", closing socket"
                << std::endl;
      stop();
      return false;
    }
  }

  return true;
}

ReadOnlyClient::ReadOnlyClient(std::unique_ptr<platform::Socket> socket,
                               Request* request, size_t max_buffer)
    : AbstractClient(std::move(socket), request, false, max_buffer) {}

void ReadOnlyClient::handleIncomingStream(const uint8_t* data, size_t len) {
  (void)data;
  (void)len;
  // Read-only clients don't process inbound socket data for bus requests
}

bool ReadOnlyClient::hasPendingIncomingData() const {
  return false;  // Read-only clients never generate bus requests
}

bool ReadOnlyClient::popPendingIncomingData(uint8_t& out) {
  (void)out;
  return false;  // Never called for read-only clients
}

BridgeAction ReadOnlyClient::onBusByte(const BusEventInfo&) {
  if (!isConnected()) return BridgeAction::stop_session;
  // This should never happen, because ReadOnlyClient is never allowed to write
  return BridgeAction::stop_session;
}

void ReadOnlyClient::enqueueOutgoingData(ByteView data) {
  if (!socket_ || !socket_->isValid() || data.empty()) return;
  {
    platform::LockGuard<platform::Mutex> lock(io_mutex_);

    // Drop oldest data if we would exceed capacity (O(1) in circular buffer)
    if (count_ + data.size() > max_buffer_size_) {
      size_t to_drop = (count_ + data.size()) - max_buffer_size_;
      head_ = (head_ + to_drop) % max_buffer_size_;
      count_ -= to_drop;
    }

    for (uint8_t b : data) {
      outbound_buffer_[(head_ + count_) % max_buffer_size_] = b;
      count_++;
    }
  }
}

ClientInfo ReadOnlyClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  return ClientInfo{socket_ ? socket_->getFd() : -1, "read_only", isConnected(),
                    write_capable_, count_};
}

RegularClient::RegularClient(std::unique_ptr<platform::Socket> socket,
                             Request* request, size_t max_buffer)
    : AbstractClient(std::move(socket), request, true, max_buffer) {}

void RegularClient::handleIncomingStream(const uint8_t* data, size_t len) {
  if (!isConnected() || !write_capable_ || len == 0) return;
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  // Queue all received bytes for byte-by-byte forwarding
  for (size_t i = 0; i < len; ++i) {
    inbound_buffer_.push(data[i]);
  }
}

bool RegularClient::hasPendingIncomingData() const {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  return !inbound_buffer_.empty();
}

bool RegularClient::popPendingIncomingData(uint8_t& out) {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  if (inbound_buffer_.empty()) return false;
  inbound_buffer_.pop(out);
  last_sent_byte_ = out;
  return true;
}

BridgeAction RegularClient::onBusByte(const BusEventInfo& info) {
  if (!isConnected()) return BridgeAction::stop_session;

  // // ONE-SHOT SYN filter: Hide first SYN after requestBus() (spec: 5.1, 6.1)
  // if (info.byte == ebus::Symbols::syn && filter_next_syn_) {
  //   filter_next_syn_ = false;  // Disarm after first SYN
  //   return BridgeAction::keep_active;
  // }

  // Lock to protect last_sent_byte_ and for consistency with other methods
  platform::UniqueLock<platform::Mutex> lock(io_mutex_);

  switch (info.result) {
    case RequestResult::first_won:
    case RequestResult::second_won:
      // Arbitration won: send address echo back to client and proceed to data
      lock.unlock();  // Release lock before calling enqueueOutgoingData
      enqueueOutgoingData(ByteView(&info.byte, 1));
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
      lock.unlock();  // Release lock before calling enqueueOutgoingData
      enqueueOutgoingData(ByteView(&info.byte, 1));
      return BridgeAction::bypass_wait;

    default:
      // Sniffing heartbeats (SYN) or transparent traffic
      lock.unlock();  // Release lock before calling enqueueOutgoingData
      enqueueOutgoingData(ByteView(&info.byte, 1));
      return BridgeAction::keep_active;
  }
}

void RegularClient::enqueueOutgoingData(ByteView data) {
  if (!socket_ || !socket_->isValid() || data.empty()) return;
  {
    platform::LockGuard<platform::Mutex> lock(io_mutex_);

    if (count_ + data.size() > max_buffer_size_) {
      size_t to_drop = (count_ + data.size()) - max_buffer_size_;
      head_ = (head_ + to_drop) % max_buffer_size_;
      count_ -= to_drop;
    }

    for (uint8_t b : data) {
      outbound_buffer_[(head_ + count_) % max_buffer_size_] = b;
      count_++;
    }
  }
}

ClientInfo RegularClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  return ClientInfo{socket_ ? socket_->getFd() : -1, "regular", isConnected(),
                    write_capable_, count_};
}

EnhancedClient::EnhancedClient(std::unique_ptr<platform::Socket> socket,
                               Request* request, size_t max_buffer)
    : AbstractClient(std::move(socket), request, true, max_buffer) {}

void EnhancedClient::onSessionStart(uint32_t session_id) {
  (void)session_id;
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  incoming_len_ = 0;
}

void EnhancedClient::handleIncomingStream(const uint8_t* data, size_t len) {
  if (!isConnected() || len == 0) return;

  bool need_error_response = false;
  bool need_reset_response = false;
  bool need_info_response = false;
  enhanced::Response response_type = enhanced::Response::received;
  uint8_t response_val = 0;

  {
    platform::UniqueLock<platform::Mutex> lock(io_mutex_);

    for (size_t i = 0; i < len; ++i) {
      uint8_t b = data[i];
      incoming_buf_[incoming_len_++] = b;

      // Check if we have a short-form command (single byte < 0x80)
      if (incoming_buf_[0] < detail::EnhancedProtocolLimits::data_threshold) {
        if (write_capable_) {
          inbound_buffer_.push(incoming_buf_[0]);
        }
        incoming_len_ = 0;
        continue;
      }

      // Need at least 2 bytes for enhanced sequences
      if (incoming_len_ < detail::EnhancedProtocolLimits::max_sequence_len)
        continue;

      if (!enhanced::Protocol::isValidSequence(incoming_buf_[0],
                                               incoming_buf_[1])) {
        // Disconnect client on protocol error (but send error response first)
        incoming_len_ = 0;
        lock.unlock();  // Release lock before sending error and stopping
        createEnhancedResponse(enhanced::Response::error_host,
                               static_cast<uint8_t>(enhanced::Error::framing));
        stop();
        return;  // Exit early since client is disconnected
      }

      enhanced::Command cmd;
      uint8_t data_val;
      enhanced::Protocol::decode(incoming_buf_, cmd, data_val);
      incoming_len_ = 0;

      show(">", static_cast<uint8_t>(cmd), data_val, incoming_buf_);

      switch (cmd) {
        case enhanced::Command::init:
          need_reset_response = true;
          break;
        case enhanced::Command::send:
          if (write_capable_) {
            inbound_buffer_.push(data_val);
            last_sent_byte_ = data_val;
          }
          break;
        case enhanced::Command::start:
          if (data_val == ebus::Symbols::syn && request_) {
            request_->reset();
          } else if (write_capable_) {
            inbound_buffer_.push(data_val);
            last_sent_byte_ = data_val;
          }
          break;
        case enhanced::Command::info:
          need_info_response = true;
          break;
        default:
          break;
      }
    }
  }

  // Send responses outside the lock to avoid deadlock with enqueueOutgoingData
  if (need_error_response) {
    createEnhancedResponse(response_type, response_val);
  }
  if (need_reset_response) {
    createEnhancedResponse(enhanced::Response::resetted, 0x00);
  }
  if (need_info_response) {
    createEnhancedResponse(enhanced::Response::info, 0x08);
    createEnhancedResponse(enhanced::Response::info, 0x11);
    createEnhancedResponse(enhanced::Response::info, 0x07);
    createEnhancedResponse(enhanced::Response::info, 0x9a);
    createEnhancedResponse(enhanced::Response::info, 0xeb);
    createEnhancedResponse(enhanced::Response::info, 0x0b);
    createEnhancedResponse(enhanced::Response::info, 0x21);
    createEnhancedResponse(enhanced::Response::info, 0xc4);
    createEnhancedResponse(enhanced::Response::info, 0x31);
  }
}

bool EnhancedClient::hasPendingIncomingData() const {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  return !inbound_buffer_.empty();
}

bool EnhancedClient::popPendingIncomingData(uint8_t& out) {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  if (inbound_buffer_.empty()) return false;
  inbound_buffer_.pop(out);
  last_sent_byte_ = out;
  return true;
}

BridgeAction EnhancedClient::onBusByte(const BusEventInfo& info) {
  if (!isConnected()) return BridgeAction::stop_session;

  // // ONE-SHOT SYN filter: Hide first SYN after requestBus() (spec: 5.1, 6.1)
  // if (info.byte == ebus::Symbols::syn && filter_next_syn_) {
  //   filter_next_syn_ = false;  // Disarm after first SYN
  //   return BridgeAction::keep_active;
  // }

  // Need to lock because last_sent_byte_ is accessed from handleIncomingStream
  // (different thread)
  platform::UniqueLock<platform::Mutex> lock(io_mutex_);

  switch (info.result) {
    case RequestResult::first_lost:
    case RequestResult::second_lost:
      // Arbitration lost: return 0x0A + the master address that actually won
      {
        uint8_t byte = info.byte;
        lock.unlock();  // Release lock before calling createEnhancedResponse
        createEnhancedResponse(enhanced::Response::failed, byte);
      }
      return BridgeAction::stop_session;
    case RequestResult::first_error:
    case RequestResult::retry_error:
    case RequestResult::second_error:
      // Physical layer error
      lock.unlock();  // Release lock before calling createEnhancedResponse
      createEnhancedResponse(enhanced::Response::error_ebus,
                             static_cast<uint8_t>(enhanced::Error::framing));
      return BridgeAction::stop_session;
    case RequestResult::first_won:
    case RequestResult::second_won:
      // Arbitration won: signal started
      {
        uint8_t byte = info.byte;
        lock.unlock();  // Release lock before calling createEnhancedResponse
        createEnhancedResponse(enhanced::Response::started, byte);
      }
      return BridgeAction::bypass_wait;
    case RequestResult::observe_data:
      // Verification vs Sniffing
      {
        uint8_t byte = info.byte;
        bool match = (byte == last_sent_byte_);
        lock.unlock();  // Release lock before calling createEnhancedResponse
        createEnhancedResponse(enhanced::Response::received, byte);
        if (match) {
          return BridgeAction::bypass_wait;
        }
        return BridgeAction::keep_active;
      }

    default:
      // Sniffing (SYN, retry steps, etc.)
      {
        uint8_t byte = info.byte;
        lock.unlock();  // Release lock before calling createEnhancedResponse
        createEnhancedResponse(enhanced::Response::received, byte);
      }
      return BridgeAction::keep_active;
  }
}

void EnhancedClient::enqueueOutgoingData(ByteView data) {
  if (!socket_ || !socket_->isValid() || data.empty()) return;

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
    platform::LockGuard<platform::Mutex> lock(io_mutex_);

    constexpr size_t seq_len = detail::EnhancedProtocolLimits::max_sequence_len;
    auto pushByte = [this](uint8_t b) {
      if (count_ >= max_buffer_size_) {
        head_ = (head_ + 1) % max_buffer_size_;
        count_--;
      }
      outbound_buffer_[(head_ + count_) % max_buffer_size_] = b;
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
      show("<", cmd, val, out);
    }
  }
}

ClientInfo EnhancedClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(io_mutex_);
  return ClientInfo{socket_ ? socket_->getFd() : -1, "enhanced", isConnected(),
                    write_capable_, count_};
}

void EnhancedClient::createEnhancedResponse(enhanced::Response res,
                                            uint8_t val) {
  const uint8_t raw[2] = {static_cast<uint8_t>(res), val};
  enqueueOutgoingData(ByteView(raw, 2));
}

std::unique_ptr<AbstractClient> createClient(
    std::unique_ptr<platform::Socket> socket, Request* req, ClientType type,
    size_t max_buffer) {
  switch (type) {
    case ClientType::read_only:
      return std::unique_ptr<AbstractClient>(
          new ReadOnlyClient(std::move(socket), req, max_buffer));
    case ClientType::regular:
      return std::unique_ptr<AbstractClient>(
          new RegularClient(std::move(socket), req, max_buffer));
    case ClientType::enhanced:
      return std::unique_ptr<AbstractClient>(
          new EnhancedClient(std::move(socket), req, max_buffer));
    default:
      return nullptr;
  }
}

}  // namespace ebus::detail