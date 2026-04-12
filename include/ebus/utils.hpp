/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ebus/definitions.hpp"

namespace ebus {

// --- Address Logic ---
bool isMaster(const uint8_t& byte);
bool isSlave(const uint8_t& byte);
bool isTarget(const uint8_t& byte);

uint8_t masterOf(const uint8_t& byte);
uint8_t slaveOf(const uint8_t& byte);

// --- Hex and String Conversion ---
const std::string toString(const uint8_t& byte);
const std::string toString(const std::vector<uint8_t>& vec);
const std::vector<uint8_t> toVector(const std::string& str);

// --- Vector Helpers ---
const std::vector<uint8_t> range(const std::vector<uint8_t>& vec,
                                 const size_t& index, const size_t& len);

bool contains(const std::vector<uint8_t>& vec,
              const std::vector<uint8_t>& search);

bool matches(const std::vector<uint8_t>& vec,
             const std::vector<uint8_t>& search, size_t index = 0);

// --- Protocol Math ---

/**
 * Calculates the eBUS 8-bit CRC using the 0x9b polynomial.
 */
uint8_t calcCrc(const uint8_t& byte, const uint8_t& init);

}  // namespace ebus