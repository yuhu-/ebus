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
      buffer_storage_(new uint8_t[max_buffer]),
      request_(request),
      max_buffer_size_(max_buffer),
      write_capable_(write_capable) {}

AbstractClient::~AbstractClient() { stop(); }

void AbstractClient::stop() {
  if (socket_ && socket_->isValid()) {
    socket_->close();
  }
}

bool AbstractClient::tryFlushOutboundBuffer() {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
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
    ssize_t n = socket_->write(&buffer_storage_[head_], part_len);

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
        // Expected on non-blocking socket: buffer full or interrupted, try later
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

void ReadOnlyClient::processIncomingData(const uint8_t* data, size_t len) {
  (void)data;
  (void)len;
  // Read-only clients don't process inbound socket data for bus requests
}

bool ReadOnlyClient::hasPendingBusRequest() const {
  return false;  // Read-only clients never generate bus requests
}

bool ReadOnlyClient::popPendingBusRequest(uint8_t& out) {
  (void)out;
  return false;  // Never called for read-only clients
}

void ReadOnlyClient::sendToClient(ByteView data) {
  if (!socket_ || !socket_->isValid() || data.empty()) return;
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
  return ClientInfo{socket_ ? socket_->getFd() : -1, "read_only", isConnected(),
                    write_capable_, count_};
}

RegularClient::RegularClient(std::unique_ptr<platform::Socket> socket,
                             Request* request, size_t max_buffer)
    : AbstractClient(std::move(socket), request, true, max_buffer) {}

void RegularClient::processIncomingData(const uint8_t* data, size_t len) {
  if (!isConnected() || !write_capable_ || len == 0) return;
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  // Queue all received bytes for byte-by-byte forwarding
  for (size_t i = 0; i < len; ++i) {
    pending_bus_requests_.push(data[i]);
  }
}

bool RegularClient::hasPendingBusRequest() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return !pending_bus_requests_.empty();
}

bool RegularClient::popPendingBusRequest(uint8_t& out) {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  if (pending_bus_requests_.empty()) return false;
  pending_bus_requests_.pop(out);
  last_sent_byte_ = out;
  return true;
}

void RegularClient::sendToClient(ByteView data) {
  if (!socket_ || !socket_->isValid() || data.empty()) return;
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

  // ONE-SHOT SYN filter: Hide first SYN after requestBus() (spec: 5.1, 6.1)
  if (info.byte == ebus::Symbols::syn && filter_next_syn_) {
    filter_next_syn_ = false;  // Disarm after first SYN
    return BridgeAction::keep_active;
  }

  // Lock to protect last_sent_byte_ and for consistency with other methods
  platform::UniqueLock<platform::Mutex> lock(buffer_mutex_);

  switch (info.result) {
    case RequestResult::first_won:
    case RequestResult::second_won:
      // Arbitration won: send address echo back to client and proceed to data
      lock.unlock();  // Release lock before calling sendToClient
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
      lock.unlock();  // Release lock before calling sendToClient
      sendToClient(ByteView(&info.byte, 1));
      return BridgeAction::bypass_wait;

    default:
      // Sniffing heartbeats (SYN) or transparent traffic
      lock.unlock();  // Release lock before calling sendToClient
      sendToClient(ByteView(&info.byte, 1));
      return BridgeAction::keep_active;
  }
}

ClientInfo RegularClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return ClientInfo{socket_ ? socket_->getFd() : -1, "regular", isConnected(),
                    write_capable_, count_};
}

EnhancedClient::EnhancedClient(std::unique_ptr<platform::Socket> socket,
                               Request* request, size_t max_buffer)
    : AbstractClient(std::move(socket), request, true, max_buffer) {}

void EnhancedClient::onSessionStart(uint32_t session_id) {
  (void)session_id;
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  inbound_len_ = 0;
}

void EnhancedClient::processIncomingData(const uint8_t* data, size_t len) {
  if (!isConnected() || len == 0) return;

  bool need_error_response = false;
  bool need_reset_response = false;
  bool need_info_response = false;
  enhanced::Response response_type = enhanced::Response::received;
  uint8_t response_val = 0;

  {
    platform::UniqueLock<platform::Mutex> lock(buffer_mutex_);

    for (size_t i = 0; i < len; ++i) {
      uint8_t b = data[i];
      inbound_buf_[inbound_len_++] = b;

      // Check if we have a short-form command (single byte < 0x80)
      if (inbound_buf_[0] < detail::EnhancedProtocolLimits::data_threshold) {
        if (write_capable_) {
          pending_bus_requests_.push(inbound_buf_[0]);
        }
        inbound_len_ = 0;
        continue;
      }

      // Need at least 2 bytes for enhanced sequences
      if (inbound_len_ < detail::EnhancedProtocolLimits::max_sequence_len)
        continue;

      if (!enhanced::Protocol::isValidSequence(inbound_buf_[0],
                                               inbound_buf_[1])) {
        // Disconnect client on protocol error (but send error response first)
        inbound_len_ = 0;
        lock.unlock();  // Release lock before sending error and stopping
        sendEnhancedResponse(enhanced::Response::error_host,
                             static_cast<uint8_t>(enhanced::Error::framing));
        stop();
        return;  // Exit early since client is disconnected
      }

      enhanced::Command cmd;
      uint8_t data_val;
      enhanced::Protocol::decode(inbound_buf_, cmd, data_val);
      inbound_len_ = 0;

      show(">", static_cast<uint8_t>(cmd), data_val, inbound_buf_);

      switch (cmd) {
        case enhanced::Command::init:
          need_reset_response = true;
          break;
        case enhanced::Command::send:
          if (write_capable_) {
            pending_bus_requests_.push(data_val);
            // last_sent_byte_ = data_val;
          }
          break;
        case enhanced::Command::start:
          if (data_val == ebus::Symbols::syn && request_) {
            request_->reset();
          } else if (write_capable_) {
            pending_bus_requests_.push(data_val);
            // last_sent_byte_ = data_val;
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

  // Send responses outside the lock to avoid deadlock with sendToClient
  if (need_error_response) {
    sendEnhancedResponse(response_type, response_val);
  }
  if (need_reset_response) {
    sendEnhancedResponse(enhanced::Response::resetted, 0x00);
  }
  if (need_info_response) {
    sendEnhancedResponse(enhanced::Response::info, 0x08);
    sendEnhancedResponse(enhanced::Response::info, 0x11);
    sendEnhancedResponse(enhanced::Response::info, 0x07);
    sendEnhancedResponse(enhanced::Response::info, 0x9a);
    sendEnhancedResponse(enhanced::Response::info, 0xeb);
    sendEnhancedResponse(enhanced::Response::info, 0x0b);
    sendEnhancedResponse(enhanced::Response::info, 0x21);
    sendEnhancedResponse(enhanced::Response::info, 0xc4);
    sendEnhancedResponse(enhanced::Response::info, 0x31);
  }
}

bool EnhancedClient::hasPendingBusRequest() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return !pending_bus_requests_.empty();
}

bool EnhancedClient::popPendingBusRequest(uint8_t& out) {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  if (pending_bus_requests_.empty()) return false;
  pending_bus_requests_.pop(out);
  last_sent_byte_ = out;
  return true;
}

void EnhancedClient::sendToClient(ByteView data) {
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
      show("<", cmd, val, out);
    }
    flushLocked();
  }
}

BridgeAction EnhancedClient::onBusByte(const BusEventInfo& info) {
  if (!isConnected()) return BridgeAction::stop_session;

  // ONE-SHOT SYN filter: Hide first SYN after requestBus() (spec: 5.1, 6.1)
  if (info.byte == ebus::Symbols::syn && filter_next_syn_) {
    filter_next_syn_ = false;  // Disarm after first SYN
    return BridgeAction::keep_active;
  }

  // Need to lock because last_sent_byte_ is accessed from processIncomingData
  // (different thread)
  platform::UniqueLock<platform::Mutex> lock(buffer_mutex_);

  switch (info.result) {
    case RequestResult::first_lost:
    case RequestResult::second_lost:
      // Arbitration lost: return 0x0A + the master address that actually won
      {
        uint8_t byte = info.byte;
        lock.unlock();  // Release lock before calling sendEnhancedResponse
        sendEnhancedResponse(enhanced::Response::failed, byte);
      }
      return BridgeAction::stop_session;
    case RequestResult::first_error:
    case RequestResult::retry_error:
    case RequestResult::second_error:
      // Physical layer error
      lock.unlock();  // Release lock before calling sendEnhancedResponse
      sendEnhancedResponse(enhanced::Response::error_ebus,
                           static_cast<uint8_t>(enhanced::Error::framing));
      return BridgeAction::stop_session;
    case RequestResult::first_won:
    case RequestResult::second_won:
      // Arbitration won: signal started
      {
        uint8_t byte = info.byte;
        lock.unlock();  // Release lock before calling sendEnhancedResponse
        sendEnhancedResponse(enhanced::Response::started, byte);
      }
      return BridgeAction::bypass_wait;
    case RequestResult::observe_data:
      // Verification vs Sniffing
      {
        uint8_t byte = info.byte;
        bool match = (byte == last_sent_byte_);
        lock.unlock();  // Release lock before calling sendEnhancedResponse
        sendEnhancedResponse(enhanced::Response::received, byte);
        if (match) {
          return BridgeAction::bypass_wait;
        }
        return BridgeAction::keep_active;
      }

    default:
      // Sniffing (SYN, retry steps, etc.)
      {
        uint8_t byte = info.byte;
        lock.unlock();  // Release lock before calling sendEnhancedResponse
        sendEnhancedResponse(enhanced::Response::received, byte);
      }
      return BridgeAction::keep_active;
  }
}

ClientInfo EnhancedClient::getClientInfo() const {
  platform::LockGuard<platform::Mutex> lock(buffer_mutex_);
  return ClientInfo{socket_ ? socket_->getFd() : -1, "enhanced", isConnected(),
                    write_capable_, count_};
}

void EnhancedClient::sendEnhancedResponse(enhanced::Response res, uint8_t val) {
  const uint8_t raw[2] = {static_cast<uint8_t>(res), val};
  sendToClient(ByteView(raw, 2));
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