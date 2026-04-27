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
#elif defined(POSIX)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace ebus::detail::platform {

enum class Flags { none = 0, dont_wait, peek };

inline void setNonBlocking(int fd) {
#if defined(POSIX) || defined(ESP_PLATFORM)
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

inline void close(int fd) {
  if (fd < 0) return;
#if defined(POSIX) || defined(ESP_PLATFORM)
  int type;
  socklen_t optlen = sizeof(type);
  if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &optlen) == 0) {
    ::shutdown(fd, SHUT_RDWR);
  }
#endif

#if defined(ESP_PLATFORM)
  lwip_close(fd);
#elif defined(POSIX)
  ::close(fd);
#endif
}

inline ssize_t send(int fd, const void* buf, size_t len, Flags flags) {
  int f = 0;
#if defined(POSIX) || defined(ESP_PLATFORM)
  if (flags == Flags::dont_wait) f = MSG_DONTWAIT;
  return ::send(fd, buf, len, f);
#else
  return -1;
#endif
}

inline ssize_t recv(int fd, void* buf, size_t len, Flags flags) {
  int f = 0;
#if defined(POSIX) || defined(ESP_PLATFORM)
  if (flags == Flags::dont_wait) f = MSG_DONTWAIT;
  if (flags == Flags::peek) f = MSG_PEEK | MSG_DONTWAIT;
  return ::recv(fd, buf, len, f);
#else
  return -1;
#endif
}

inline bool isInterrupted() {
#if defined(POSIX) || defined(ESP_PLATFORM)
  return errno == EINTR;
#else
  return false;
#endif
}

inline bool isWouldBlock() {
#if defined(POSIX) || defined(ESP_PLATFORM)
  return errno == EAGAIN || errno == EWOULDBLOCK;
#else
  return false;
#endif
}

}  // namespace ebus::detail::platform
