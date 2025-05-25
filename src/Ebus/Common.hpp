/*
 * Copyright (C) 2025 Roland Jax
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

// Common ebus defines and functions

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ebus {

// symbols
constexpr uint8_t sym_zero = 0x00;     // zero byte
constexpr uint8_t sym_syn = 0xaa;      // synchronization byte
constexpr uint8_t sym_ext = 0xa9;      // extend byte
constexpr uint8_t sym_syn_ext = 0x01;  // extended synchronization byte
constexpr uint8_t sym_ext_ext = 0x00;  // extended extend byte

constexpr uint8_t sym_ack = 0x00;    // positive acknowledge
constexpr uint8_t sym_nak = 0xff;    // negative acknowledge
constexpr uint8_t sym_broad = 0xfe;  // broadcast destination address

// sizes
constexpr uint8_t max_bytes = 0x10;  // 16 maximum data bytes

bool isMaster(const uint8_t &byte);
bool isSlave(const uint8_t &byte);

bool isTarget(const uint8_t &byte);

uint8_t masterOf(const uint8_t &byte);
uint8_t slaveOf(const uint8_t &byte);

const std::string to_string(const uint8_t &byte);
const std::string to_string(const std::vector<uint8_t> &vec);

const std::vector<uint8_t> to_vector(const std::string &str);

const std::vector<uint8_t> range(const std::vector<uint8_t> &vec,
                                 const size_t &index, const size_t &len);

bool contains(const std::vector<uint8_t> &vec,
              const std::vector<uint8_t> &search);

uint8_t calc_crc(const uint8_t &byte, const uint8_t &init);

}  // namespace ebus
