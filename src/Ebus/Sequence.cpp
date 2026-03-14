/*
 * Copyright (C) 2012-2026 Roland Jax
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

ebus::Sequence::Sequence(const Sequence& sequence, const size_t index,
                         size_t len) {
  if (len == 0) len = sequence.size() - index;

  sequence_.resize(len);
  std::copy(sequence.sequence_.begin() + index,
            sequence.sequence_.begin() + index + len, sequence_.begin());

  extended_ = sequence.extended_;
}

void ebus::Sequence::assign(const std::vector<uint8_t>& vec,
                            const bool extended) {
  clear();
  sequence_ = vec;
  extended_ = extended;
}

void ebus::Sequence::push_back(const uint8_t byte, const bool extended) {
  sequence_.push_back(byte);
  extended_ = extended;
}

const uint8_t& ebus::Sequence::operator[](const size_t index) const {
  return sequence_.at(index);
}

const std::vector<uint8_t> ebus::Sequence::range(const size_t index,
                                                 const size_t len) const {
  return ebus::range(sequence_, index, len);
}

size_t ebus::Sequence::size() const { return sequence_.size(); }

void ebus::Sequence::clear() {
  sequence_.clear();
  extended_ = false;
}

uint8_t ebus::Sequence::crc() {
  if (!extended_) extend();

  uint8_t crc = sym_zero;

  for (size_t i = 0; i < sequence_.size(); i++)
    crc = calc_crc(sequence_.at(i), crc);

  reduce();

  return crc;
}

void ebus::Sequence::extend() {
  if (extended_) return;

  // maximum possible size (worst case: every byte expands to 2)
  size_t max_size = sequence_.size() * 2;
  std::vector<uint8_t> tmp(max_size);
  size_t j = 0;

  for (size_t i = 0; i < sequence_.size(); i++) {
    if (sequence_[i] == sym_syn) {
      tmp[j++] = sym_ext;
      tmp[j++] = sym_syn_ext;
    } else if (sequence_[i] == sym_ext) {
      tmp[j++] = sym_ext;
      tmp[j++] = sym_ext_ext;
    } else {
      tmp[j++] = sequence_[i];
    }
  }
  tmp.resize(j);  // shrink to actual size

  sequence_ = std::move(tmp);
  extended_ = true;
}

void ebus::Sequence::reduce() {
  if (!extended_) return;

  // In the worst case, the reduced sequence is at most as large as sequence
  std::vector<uint8_t> tmp(sequence_.size());
  size_t j = 0;
  bool extended = false;

  for (size_t i = 0; i < sequence_.size(); i++) {
    if (sequence_[i] == sym_syn || sequence_[i] == sym_ext) {
      extended = true;
    } else if (extended) {
      if (sequence_[i] == sym_syn_ext)
        tmp[j++] = sym_syn;
      else
        tmp[j++] = sym_ext;
      extended = false;
    } else {
      tmp[j++] = sequence_[i];
    }
  }
  tmp.resize(j);  // shrink to actual size

  sequence_ = std::move(tmp);
  extended_ = false;
}

const std::string ebus::Sequence::to_string() const {
  if (sequence_.empty()) return {};

  std::ostringstream ostr;
  ostr << std::hex << std::setfill('0');

  for (size_t i = 0; i < sequence_.size(); ++i)
    ostr << std::nouppercase << std::setw(2)
         << static_cast<unsigned>(sequence_[i]);

  return ostr.str();
}

const std::vector<uint8_t>& ebus::Sequence::to_vector() const {
  return sequence_;
}
