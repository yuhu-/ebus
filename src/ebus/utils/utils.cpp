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

std::string ebus::toString(const std::vector<uint8_t>& vec) {
  std::ostringstream ostr;

  for (size_t i = 0; i < vec.size(); i++)
    ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned>(vec[i]);

  return ostr.str();
}

std::vector<uint8_t> ebus::toVector(const std::string& str) {
  std::vector<uint8_t> result;

  for (size_t i = 0; i + 1 < str.size(); i += 2)
    result.push_back(
        uint8_t(std::strtoul(str.substr(i, 2).c_str(), nullptr, 16)));

  return result;
}

std::vector<uint8_t> ebus::range(const std::vector<uint8_t>& vec, size_t index,
                                 size_t len) {
  if (index >= vec.size()) return {};
  size_t end = std::min(index + len, vec.size());
  return std::vector<uint8_t>(vec.begin() + index, vec.begin() + end);
}

bool ebus::contains(const std::vector<uint8_t>& vec,
                    const std::vector<uint8_t>& search) {
  if (search.empty() || vec.empty() || search.size() > vec.size()) return false;
  return std::search(vec.begin(), vec.end(), search.begin(), search.end()) !=
         vec.end();
}

bool ebus::matches(const std::vector<uint8_t>& vec,
                   const std::vector<uint8_t>& search, size_t index) {
  if (search.empty()) return true;
  if (index + search.size() > vec.size()) return false;

  if (!std::equal(search.begin(), search.end(), vec.begin() + index))
    return false;

  return true;
}
