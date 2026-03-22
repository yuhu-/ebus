/*
 * Copyright (C) 2025-2026 Roland Jax
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#if defined(POSIX)
#include "BusPosix.hpp"

#include <algorithm>
#include <iostream>

#include "../Common.hpp"

ebus::BusPosix::BusPosix(const busConfig& config, Request* request)
    : device_(config.device),
      simulate_(config.simulate),
      enableSyn_(config.enable_syn),
      masterAddr_(config.master_addr),
      synBase_(config.syn_base),
      synTolerance_(config.syn_tolerance),
      synDeterministic_(config.syn_deterministic),
      request_(request),
      fd_(-1),
      open_(false),
      byteQueue_(new Queue<BusEvent>()),
      thread_(),
      running_(false) {}

ebus::BusPosix::~BusPosix() { stop(); }

void ebus::BusPosix::start() {
  if (open_) return;

  if (simulate_) {
    if (::pipe(pipeFds_) < 0)
      throw std::runtime_error("Failed to create simulation pipe");
    fd_ = pipeFds_[0];
    open_ = true;
  } else {
    struct termios newSettings;
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0 || isatty(fd_) == 0)
      throw std::runtime_error("Failed to open ebus device: " + device_);

    tcgetattr(fd_, &oldSettings_);
    ::memset(&newSettings, 0, sizeof(newSettings));
    newSettings.c_cflag |= (B2400 | CS8 | CLOCAL | CREAD);
    newSettings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newSettings.c_iflag |= IGNPAR;
    newSettings.c_oflag &= ~OPOST;
    newSettings.c_cc[VMIN] = 1;
    newSettings.c_cc[VTIME] = 0;

    tcflush(fd_, TCIFLUSH);
    tcsetattr(fd_, TCSAFLUSH, &newSettings);
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) & ~O_NONBLOCK);

    open_ = true;
  }

  running_.store(true);
  thread_ = std::thread(&BusPosix::readerThread, this);

  // start SYN generator if enabled in config
  if (enableSyn_) {
    currentTunique_ = synBase_ + std::chrono::milliseconds(masterAddr_ * 10) + synTolerance_;
    nextSynExpiry_ = std::chrono::steady_clock::now() + currentTunique_;

    synActive_ = false;
    synRunning_.store(true);
    synThread_ = std::thread(&BusPosix::synThread, this);
  }
}

void ebus::BusPosix::stop() {
  if (!open_) return;

  running_.store(false);
  synRunning_.store(false);

  {
    std::lock_guard<std::mutex> lock(synMutex_);
    synCv_.notify_all();
  }

  if (simulate_) {
    if (pipeFds_[1] != -1) {
      ::close(pipeFds_[1]);
      pipeFds_[1] = -1;
    }
  } else {
    if (fd_ != -1) ::tcflush(fd_, TCIOFLUSH);
  }

  if (synThread_.joinable()) synThread_.join();

  if (thread_.joinable()) thread_.join();

  if (simulate_) {
    if (pipeFds_[0] != -1) {
      ::close(pipeFds_[0]);
      pipeFds_[0] = -1;
    }
  } else if (fd_ != -1) {
    ::tcsetattr(fd_, TCSANOW, &oldSettings_);
    ::close(fd_);
  }

  fd_ = -1;
  open_ = false;
}

ebus::Queue<ebus::BusEvent>* ebus::BusPosix::getQueue() const {
  return byteQueue_.get();
}

void ebus::BusPosix::writeByte(const uint8_t byte) {
  if (simulate_) {
    lastWrittenByte_ = byte;
    writtenBytes_.push_back(byte);

    if (pipeFds_[1] != -1) {
      ::write(pipeFds_[1], &byte, 1);
    }
    return;
  }

  ensureOpen();
  if (::write(fd_, &byte, 1) == -1)
    throw std::runtime_error("BusPosix: write error");
}

void ebus::BusPosix::setWindow(const uint16_t window) { window_ = window; }

void ebus::BusPosix::setOffset(const uint16_t offset) { offset_ = offset; }

uint8_t ebus::BusPosix::getLastWrittenByte() const { return lastWrittenByte_; }

std::string ebus::BusPosix::getSimulatedWrittenBytes() const {
  return ebus::to_string(writtenBytes_);
}

void ebus::BusPosix::ensureOpen() const {
  if (!open_ || fd_ < 0) throw std::runtime_error("BusPosix: device not open");
}

void ebus::BusPosix::readerThread() {
  bool busRequestFlag = false;
  while (running_.load()) {
    uint8_t byte;
    ssize_t n = ::read(fd_, &byte, 1);
    if (n == 1) {
      // Notify SYN generator that a symbol was recognised (end of char)
      resetSynTimer(byte);

      BusEvent event;
      event.byte = byte;
      event.busRequest = busRequestFlag;
      busRequestFlag = false;
      // startBit indicates if the start bit wasn't in the expected window
      // event.startBit = true;

      if (byteQueue_) byteQueue_->push(event);

      // TODO calculate timing to write address at the right time, currently
      // just write immediately after receiving SYN
      if (byte == sym_syn && request_->busRequestPending()) {
        writeByte(request_->busRequestAddress());
        busRequestFlag = true;
      }

    } else if (n == 0) {
      // EOF - stop thread
      break;
    } else {
      // read error, optionally break or continue after short sleep
      if (errno == EINTR) continue;
      break;
    }
  }
  running_.store(false);
}

void ebus::BusPosix::resetSynTimer(uint8_t byte) {
  std::lock_guard<std::mutex> lock(synMutex_);
  auto now = std::chrono::steady_clock::now();

  // Arbitration Logic:
  // If we are the active SYN generator (synActive_ is true) and we receive
  // the echo of our own SYN (byte == sym_syn), we "won" arbitration or the bus
  // is idle. We continue generating SYNs at the fast rate (synBase_).
  if (synActive_ && byte == sym_syn) {
    nextSynExpiry_ = now + synBase_;
  } else {
    synActive_ = false;
    nextSynExpiry_ = now + currentTunique_;
  }

  synCv_.notify_one();
}

void ebus::BusPosix::synThread() {
  while (synRunning_.load()) {
    std::unique_lock<std::mutex> lock(synMutex_);

    auto now = std::chrono::steady_clock::now();
    if (nextSynExpiry_ > now) {
      synCv_.wait_until(lock, nextSynExpiry_);
      continue;
    }

    // We are about to generate a SYN, mark ourselves as active
    synActive_ = true;
    lock.unlock();
    writeByte(sym_syn);

    lock.lock();
    // Safety Fallback:
    // If the timer hasn't been updated by readerThread (receiving the echo),
    // reset it to the unique (long) value as a fallback.
    // This handles cases where our write failed or the echo was corrupted.
    if (nextSynExpiry_ <= std::chrono::steady_clock::now()) {
      nextSynExpiry_ = std::chrono::steady_clock::now() + currentTunique_;
    }
  }
}

#endif
