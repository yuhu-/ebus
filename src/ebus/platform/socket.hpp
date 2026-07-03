/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#include <esp_netdb.h>
#include <esp_vfs_eventfd.h>
#include <lwip/errno.h>
#include <lwip/sockets.h>
#elif defined(POSIX)
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#endif

#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

namespace ebus::detail::platform {

enum class Flags { none = 0, dont_wait, peek };

inline bool setNonBlocking(int fd) {
  if (fd < 0) return false;
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
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
  // Exception has occurred. Broken pipe. ebusd on port 3334
#if defined(MSG_NOSIGNAL)
  f |= MSG_NOSIGNAL;
#endif
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
  if (n < 0 && errno == ENOTSOCK) {
    return ::read(fd, buf, len);
  }
  return n;
}

inline bool isInterrupted() { return errno == EINTR; }

inline bool isWouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }

class Socket {
 public:
  enum class Type : int {
    invalid = -1,
    stream = SOCK_STREAM,
    dgram = SOCK_DGRAM
  };

  explicit Socket(Type type = Type::invalid) : type_(type) {
    if (type_ == Type::invalid) {
      fd_ = -1;
      return;
    }

    int af = AF_INET;
    int sock_type = static_cast<int>(type_);
#if defined(ESP_PLATFORM)
    fd_ = ::socket(af, sock_type, 0);
    if (fd_ != -1) {
      if (!setNonBlocking(fd_)) {
        platform::close(fd_);
        fd_ = -1;
      }
    }
#elif defined(POSIX)
    fd_ = ::socket(af, sock_type | SOCK_NONBLOCK, 0);
#endif
    assert(fd_ != -1 && "Socket creation failed");
  }

  /// for internal tests only
  explicit Socket(int fd, Type type = Type::stream) : type_(type) { fd_ = fd; }

  ~Socket() {
    if (fd_ != -1) {
      close();
    }
  }

  // Disable copying
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  // Enable moving
  Socket(Socket&& other) noexcept
      : type_(std::exchange(other.type_, Type::invalid)),
        fd_(std::exchange(other.fd_, -1)) {}

  Socket& operator=(Socket&& other) noexcept {
    if (this != &other) {
      type_ = std::exchange(other.type_, Type::invalid);
      fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
  }

  // Platform-agnostic API

  int accept() {
    assert(isValid() && "Socket is not valid");
#if defined(ESP_PLATFORM)
    int client_fd = ::accept(fd_, nullptr, nullptr);
    if (client_fd != -1) {
      setNonBlocking(client_fd);
    }
    return client_fd;
#elif defined(POSIX)
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    int client_fd =
        ::accept(fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (client_fd != -1) {
      setNonBlocking(client_fd);
    }
    return client_fd;
#endif
  }

  bool bind(uint16_t port) {
    assert(isValid() && "Socket is not valid");
    assert(port != 0 && "Port cannot be zero");

    struct sockaddr_in hints = {};
    hints.sin_family = AF_INET;
    hints.sin_port = htons(port);
    hints.sin_addr.s_addr = htonl(INADDR_ANY);
    return ::bind(fd_, reinterpret_cast<const struct sockaddr*>(&hints),
                  sizeof(hints)) == 0;
  }

  bool listen(int backlog = 4) {
    assert(isValid() && "Socket is not valid");
    assert(type_ == Type::stream && "listen() only valid for stream sockets");
    return ::listen(fd_, backlog) == 0;
  }

  ssize_t read(void* buf, size_t len) {
    assert(isValid() && "Socket is not valid");
    assert(buf != nullptr && "Buffer cannot be null");
    assert(len > 0 && "Length must be positive");

#if defined(ESP_PLATFORM)
    return ::read(fd_, buf, len);
#elif defined(POSIX)
    return ::recv(fd_, buf, len, 0);
#endif
  }

  ssize_t write(const void* buf, size_t len) {
    assert(isValid() && "Socket is not valid");
    assert(buf != nullptr && "Buffer cannot be null");
    assert(len > 0 && "Length must be positive");

#if defined(ESP_PLATFORM)
    return ::write(fd_, buf, len);
#elif defined(POSIX)
    return ::send(fd_, buf, len, 0);
#endif
  }

  bool close() {
    if (!isValid()) {
      return true;
    }
    platform::close(fd_);
    fd_ = -1;
    return true;
  }

  // Socket options
  bool setOption(int level, int option, const void* value, socklen_t len) {
    assert(isValid() && "Socket is not valid");
    assert(value != nullptr && "Value cannot be null");
    assert(len > 0 && "Length must be positive");
    return ::setsockopt(fd_, level, option, value, len) == 0;
  }

  bool getOption(int level, int option, void* value, socklen_t* len) {
    assert(isValid() && "Socket is not valid");
    assert(value != nullptr && "Value cannot be null");
    assert(len != nullptr && "Length pointer cannot be null");
    return ::getsockopt(fd_, level, option, value, len) == 0;
  }

  /**
   * @brief Creates and configures a listening socket on the given port.
   * @param port The port to listen on.
   * @return A configured Socket, or an invalid Socket if setup fails.
   */
  static inline Socket createListenSocket(uint16_t port) {
    Socket sock(Type::stream);
    if (!sock.isValid()) {
      return Socket(Type::invalid);
    }

    int enable = 1;
    if (!sock.setOption(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
      return Socket(Type::invalid);
    }

    if (!sock.bind(port)) {
      return Socket(Type::invalid);
    }

    if (!sock.listen(4)) {
      return Socket(Type::invalid);
    }
    return sock;
  }

  // Non-blocking operations
  bool setNonBlocking(bool enable) {
    assert(isValid() && "Socket is not valid");
    int flags = fcntl(fd_, F_GETFL, 0);
    assert(flags != -1 && "Failed to get socket flags");
    if (enable) {
      flags |= O_NONBLOCK;
    } else {
      flags &= ~O_NONBLOCK;
    }
    return fcntl(fd_, F_SETFL, flags) == 0;
  }

  bool isValid() const { return fd_ != -1; }

  int getFd() const { return fd_; }

  Type getType() const { return type_; }

 private:
  Type type_;
  int fd_ = -1;
};

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
