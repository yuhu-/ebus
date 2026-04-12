/*
 * Copyright (C) 2025 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstdlib>
#include <ebus/utils.hpp>
#include <iomanip>
#include <sstream>

std::string ebus::toString(uint8_t byte) {
  std::ostringstream ostr;
  ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned>(byte);
  return ostr.str();
}

std::vector<uint8_t> ebus::toVector(const std::string& str) {
  std::vector<uint8_t> result;

  for (size_t i = 0; i + 1 < str.size(); i += 2)
    result.push_back(
        uint8_t(std::strtoul(str.substr(i, 2).c_str(), nullptr, 16)));

  return result;
}
