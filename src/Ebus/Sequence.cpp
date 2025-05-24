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

#include <iomanip>
#include <sstream>

#include "Common.hpp"

ebus::Sequence::Sequence(const Sequence &seq, const size_t index, size_t len) {
  if (len == 0) len = seq.size() - index;

  for (size_t i = index; i < index + len; i++) m_seq.push_back(seq.m_seq.at(i));

  m_extended = seq.m_extended;
}

void ebus::Sequence::assign(const std::vector<uint8_t> &vec,
                            const bool extended) {
  clear();

  for (size_t i = 0; i < vec.size(); i++) push_back(vec[i], extended);
}

void ebus::Sequence::push_back(const uint8_t byte, const bool extended) {
  m_seq.push_back(byte);
  m_extended = extended;
}

const uint8_t &ebus::Sequence::operator[](const size_t index) const {
  return m_seq.at(index);
}

const std::vector<uint8_t> ebus::Sequence::range(const size_t index,
                                                 const size_t len) const {
  return ebus::range(m_seq, index, len);
}

size_t ebus::Sequence::size() const { return m_seq.size(); }

void ebus::Sequence::clear() {
  m_seq.clear();
  m_seq.shrink_to_fit();
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

  std::vector<uint8_t> tmp;

  for (size_t i = 0; i < m_seq.size(); i++) {
    if (m_seq.at(i) == sym_syn) {
      tmp.push_back(sym_ext);
      tmp.push_back(sym_syn_ext);
    } else if (m_seq.at(i) == sym_ext) {
      tmp.push_back(sym_ext);
      tmp.push_back(sym_ext_ext);
    } else {
      tmp.push_back(m_seq.at(i));
    }
  }

  m_seq = tmp;
  m_extended = true;
}

void ebus::Sequence::reduce() {
  if (!m_extended) return;

  std::vector<uint8_t> tmp;
  bool extended = false;

  for (size_t i = 0; i < m_seq.size(); i++) {
    if (m_seq.at(i) == sym_syn || m_seq.at(i) == sym_ext) {
      extended = true;
    } else if (extended) {
      if (m_seq.at(i) == sym_syn_ext)
        tmp.push_back(sym_syn);
      else
        tmp.push_back(sym_ext);

      extended = false;
    } else {
      tmp.push_back(m_seq.at(i));
    }
  }

  m_seq = tmp;
  m_extended = false;
}

const std::string ebus::Sequence::to_string() const {
  std::ostringstream ostr;

  for (size_t i = 0; i < m_seq.size(); i++)
    ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned>(m_seq.at(i));

  return ostr.str();
}

const std::vector<uint8_t> &ebus::Sequence::to_vector() const { return m_seq; }
