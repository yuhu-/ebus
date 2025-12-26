/*
 * Copyright (C) 2012-2025 Roland Jax
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

#include "Sequence.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

#include "Common.hpp"

ebus::Sequence::Sequence(const Sequence& seq, const size_t index, size_t len) {
  if (len == 0) len = seq.size() - index;

  m_seq.resize(len);
  std::copy(seq.m_seq.begin() + index, seq.m_seq.begin() + index + len,
            m_seq.begin());

  m_extended = seq.m_extended;
}

void ebus::Sequence::assign(const std::vector<uint8_t>& vec,
                            const bool extended) {
  clear();
  m_seq = vec;
  m_extended = extended;
}

void ebus::Sequence::push_back(const uint8_t byte, const bool extended) {
  m_seq.push_back(byte);
  m_extended = extended;
}

const uint8_t& ebus::Sequence::operator[](const size_t index) const {
  return m_seq.at(index);
}

const std::vector<uint8_t> ebus::Sequence::range(const size_t index,
                                                 const size_t len) const {
  return ebus::range(m_seq, index, len);
}

size_t ebus::Sequence::size() const { return m_seq.size(); }

void ebus::Sequence::clear() {
  m_seq.clear();
  m_extended = false;
}

uint8_t ebus::Sequence::crc() {
  if (!m_extended) extend();

  uint8_t crc = sym_zero;

  for (size_t i = 0; i < m_seq.size(); i++) crc = calc_crc(m_seq.at(i), crc);

  reduce();

  return crc;
}

void ebus::Sequence::extend() {
  if (m_extended) return;

  // maximum possible size (worst case: every byte expands to 2)
  size_t max_size = m_seq.size() * 2;
  std::vector<uint8_t> tmp(max_size);
  size_t j = 0;

  for (size_t i = 0; i < m_seq.size(); i++) {
    if (m_seq[i] == sym_syn) {
      tmp[j++] = sym_ext;
      tmp[j++] = sym_syn_ext;
    } else if (m_seq[i] == sym_ext) {
      tmp[j++] = sym_ext;
      tmp[j++] = sym_ext_ext;
    } else {
      tmp[j++] = m_seq[i];
    }
  }
  tmp.resize(j);  // shrink to actual size

  m_seq = std::move(tmp);
  m_extended = true;
}

void ebus::Sequence::reduce() {
  if (!m_extended) return;

  // In the worst case, the reduced sequence is at most as large as m_seq
  std::vector<uint8_t> tmp(m_seq.size());
  size_t j = 0;
  bool extended = false;

  for (size_t i = 0; i < m_seq.size(); i++) {
    if (m_seq[i] == sym_syn || m_seq[i] == sym_ext) {
      extended = true;
    } else if (extended) {
      if (m_seq[i] == sym_syn_ext)
        tmp[j++] = sym_syn;
      else
        tmp[j++] = sym_ext;
      extended = false;
    } else {
      tmp[j++] = m_seq[i];
    }
  }
  tmp.resize(j);  // shrink to actual size

  m_seq = std::move(tmp);
  m_extended = false;
}

const std::string ebus::Sequence::to_string() const {
  if (m_seq.empty()) return {};

  std::ostringstream ostr;
  ostr << std::hex << std::setfill('0');

  for (size_t i = 0; i < m_seq.size(); ++i)
    ostr << std::nouppercase << std::setw(2) << static_cast<unsigned>(m_seq[i]);

  return ostr.str();
}

const std::vector<uint8_t>& ebus::Sequence::to_vector() const { return m_seq; }
