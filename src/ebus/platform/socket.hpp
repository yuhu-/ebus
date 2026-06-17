/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(ESP_PLATFORM)
#include <lwip/errno.h>
#include <lwip/sockets.h>
// #include <sys/eventfd.h>
#include <esp_vfs_eventfd.h>
#elif defined(POSIX)
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace ebus::detail::platform {

enum class Flags { none = 0, dont_wait, peek };

inline void setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief Checks if there is data waiting to be read without consuming it.
 */
inline bool hasDataAvailable(int fd) {
  int n = 0;
  if (::ioctl(fd, FIONREAD, &n) == 0) return n > 0;
  return false;
}

inline void close(int fd) {
  if (fd < 0) return;
  int type;
  socklen_t optlen = sizeof(type);
  if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &optlen) == 0) {
    ::shutdown(fd, SHUT_RDWR);
  }

#if defined(ESP_PLATFORM)
  lwip_close(fd);
#else
  ::close(fd);
#endif
}

inline ssize_t send(int fd, const void* buf, size_t len, Flags flags) {
  int f = 0;
  if (flags == Flags::dont_wait) f = MSG_DONTWAIT;
  ssize_t n = ::send(fd, buf, len, f);
  // Fallback to write for non-socket FDs (pipes in mocks)
  if (n < 0 && errno == ENOTSOCK) return ::write(fd, buf, len);
  return n;
}

inline ssize_t recv(int fd, void* buf, size_t len, Flags flags) {
  int f = 0;
  if (flags == Flags::dont_wait) f = MSG_DONTWAIT;
  if (flags == Flags::peek) f = MSG_PEEK | MSG_DONTWAIT;
  ssize_t n = ::recv(fd, buf, len, f);
  // Fallback to read for non-socket FDs (pipes in mocks)
  if (n < 0 && errno == ENOTSOCK && flags != Flags::peek)
    return ::read(fd, buf, len);
  return n;
}

inline bool isInterrupted() { return errno == EINTR; }

inline bool isWouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }

/**
 * @brief Returns true if the error indicates a hardware or protocol failure
 * that requires closing the connection.
 */
inline bool isFatalError() {
  return errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN ||
         errno == ETIMEDOUT || errno == EBADF;
}

class WakeupSignal {
 public:
  WakeupSignal() = default;
  ~WakeupSignal() { close(); }

  bool init() {
#if defined(ESP_PLATFORM)
    esp_vfs_eventfd_config_t config;
    config.max_fds = 5;

    esp_vfs_eventfd_register(&config);
    fd_ = ::eventfd(0, 0);
    if (fd_ < 0) return false;
    setNonBlocking(fd_);
    return true;
#else
    int pipefd[2];
    if (::pipe(pipefd) == -1) return false;
    read_fd_ = pipefd[0];
    write_fd_ = pipefd[1];
    setNonBlocking(read_fd_);
    setNonBlocking(write_fd_);
    return true;
#endif
  }

  void close() {
#if defined(ESP_PLATFORM)
    if (fd_ >= 0) {
      lwip_close(fd_);
      fd_ = -1;
    }
#else
    if (read_fd_ >= 0) {
      ::close(read_fd_);
      read_fd_ = -1;
    }
    if (write_fd_ >= 0) {
      ::close(write_fd_);
      write_fd_ = -1;
    }
#endif
  }

  void signal() {
#if defined(ESP_PLATFORM)
    if (fd_ >= 0) {
      uint64_t val = 1;
      ::write(fd_, &val, sizeof(val));
    }
#else
    if (write_fd_ >= 0) {
      char signal_byte = '1';
      ::write(write_fd_, &signal_byte, 1);
    }
#endif
  }

  void drain() {
#if defined(ESP_PLATFORM)
    if (fd_ >= 0) {
      uint64_t val = 0;
      ::read(fd_, &val, sizeof(val));
    }
#else
    if (read_fd_ >= 0) {
      char dummy_buf[16];
      while (::read(read_fd_, dummy_buf, sizeof(dummy_buf)) > 0 ||
             isInterrupted()) {
      }
    }
#endif
  }

  int getReadFd() const {
#if defined(ESP_PLATFORM)
    return fd_;
#else
    return read_fd_;
#endif
  }

 private:
#if defined(ESP_PLATFORM)
  int fd_ = -1;
#else
  int read_fd_ = -1;
  int write_fd_ = -1;
#endif
};

}  // namespace ebus::detail::platform
