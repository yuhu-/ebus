/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>
#include <functional>
#include <stdexcept>

namespace ebus {

/**
 * VirtualLine simulates the physical characteristics of the eBUS media.
 * It introduces propagation delays (2400 baud throttle) and provides
 * a loopback mechanism via a POSIX pipe.
 */
class VirtualLine {
 public:
  VirtualLine() {
    if (::pipe(fds_) < 0) {
      throw std::runtime_error("VirtualLine: Failed to create simulation pipe");
    }
  }

  ~VirtualLine() {
    if (fds_[0] != -1) ::close(fds_[0]);
    if (fds_[1] != -1) ::close(fds_[1]);
  }

  int getReadFd() const { return fds_[0]; }

  /**
   * Simulates the serialization of a byte onto the wire.
   * Blocks for ~4.17ms to mimic 2400 baud bit-time.
   */
  void write(uint8_t byte, const std::vector<std::function<void(const uint8_t&)>>& listeners) {
    if (fds_[1] == -1) return;

    // Throttle: Mimic 2400 baud UART delay (10 bits @ 2400bps ≈ 4.17ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(4));

    // Trigger write listeners only when the byte "hits the wire"
    for (const auto& listener : listeners) {
      listener(byte);
    }

    if (::write(fds_[1], &byte, 1) != 1) {
      // Pipe full or closed
    }
  }

  void closeWriter() {
    if (fds_[1] != -1) ::close(fds_[1]);
    fds_[1] = -1;
  }

 private:
  int fds_[2] = {-1, -1};
};

}  // namespace ebus