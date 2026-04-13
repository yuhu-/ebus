/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <unistd.h>

#include <catch2/catch_all.hpp>
#include <chrono>
#include <cstdint>
#include <ebus/data_types.hpp>
#include <ebus/definitions.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/handler.hpp"
#include "platform/system.hpp"

/**
 * Robust read helper to handle partial TCP/Socket reads.
 */
inline bool readExact(int fd, uint8_t* buffer, size_t length) {
  size_t total = 0;
  while (total < length) {
    ssize_t n = read(fd, buffer + total, length - total);
    if (n <= 0) return false;
    total += n;
  }
  return true;
}
