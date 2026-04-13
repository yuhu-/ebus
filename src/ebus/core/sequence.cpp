/*
 * Copyright (C) 2012-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/sequence.hpp"

#include <algorithm>
#include <ebus/utils.hpp>
#include <utility>

ebus::Sequence::Sequence(const Sequence& sequence, const size_t index,
                         size_t len) {
  if (index >= sequence.size()) {
    extended_ = sequence.extended_;
    return;
  }

  if (len == 0 || index + len > sequence.size()) len = sequence.size() - index;

  sequence_.reserve(len);
  sequence_.resize(len);
  std::copy(sequence.sequence_.begin() + index,
            sequence.sequence_.begin() + index + len, sequence_.begin());

  extended_ = sequence.extended_;
}

void ebus::Sequence::assign(const std::vector<uint8_t>& vec,
                            const bool extended) {
  sequence_.assign(vec.begin(), vec.end());
  extended_ = extended;
}

void ebus::Sequence::assign(const Sequence& other, size_t index, size_t len) {
  if (index >= other.size()) {
    clear();
    extended_ = other.extended_;
    return;
  }
  if (len == 0 || index + len > other.size()) len = other.size() - index;
  sequence_.assign(other.sequence_.begin() + index,
                   other.sequence_.begin() + index + len);
  extended_ = other.extended_;
}

void ebus::Sequence::pushBack(const uint8_t byte, const bool extended) {
  sequence_.push_back(byte);
  extended_ = extended;
}

bool ebus::Sequence::operator==(const Sequence& other) const {
  return sequence_ == other.sequence_ && extended_ == other.extended_;
}

bool ebus::Sequence::operator!=(const Sequence& other) const {
  return !(*this == other);
}

void ebus::Sequence::append(const Sequence& other) {
  // Ensure the appended sequence matches the current state (extended vs
  // reduced) before merging the data.
  Sequence temp = other;
  if (extended_) {
    temp.extend();
  } else {
    temp.reduce();
  }
  sequence_.insert(sequence_.end(), temp.sequence_.begin(),
                   temp.sequence_.end());
}

uint8_t ebus::Sequence::operator[](size_t index) const {
  return sequence_[index];
}

const std::vector<uint8_t> ebus::Sequence::range(const size_t index,
                                                 const size_t len) const {
  return ebus::range(sequence_, index, len);
}

void ebus::Sequence::reserve(size_t capacity) { sequence_.reserve(capacity); }

size_t ebus::Sequence::size() const { return sequence_.size(); }

void ebus::Sequence::clear() {
  sequence_.clear();
  extended_ = false;
}

uint8_t ebus::Sequence::crc() const {
  // According to eBUS spec 5.7 the CRC is calculated over the "expanded
  // transmission sequence". We calculate this on-the-fly to avoid copies.
  uint8_t current_crc = sym_zero;
  for (uint8_t byte : sequence_) {
    if (!extended_) {
      // Simulate extension logic: AA -> A9 01, A9 -> A9 00
      if (byte == sym_syn) {
        current_crc = calcCRC(sym_ext, current_crc);
        current_crc = calcCRC(sym_syn_ext, current_crc);
      } else if (byte == sym_ext) {
        current_crc = calcCRC(sym_ext, current_crc);
        current_crc = calcCRC(sym_ext_ext, current_crc);
      } else {
        current_crc = calcCRC(byte, current_crc);
      }
    } else {
      current_crc = calcCRC(byte, current_crc);
    }
  }
  return current_crc;
}

void ebus::Sequence::extend() {
  if (extended_) return;

  // 1. Calculate how many extra bytes we need
  size_t extra = 0;
  for (uint8_t b : sequence_) {
    if (b == sym_syn || b == sym_ext) ++extra;
  }

  if (extra == 0) {
    extended_ = true;
    return;
  }

  // 2. Grow vector (reuses capacity if reserved) and shift elements backwards
  size_t old_size = sequence_.size();
  size_t new_size = old_size + extra;
  sequence_.resize(new_size);

  size_t write_idx = new_size - 1;
  for (int i = static_cast<int>(old_size) - 1; i >= 0; --i) {
    uint8_t b = sequence_[i];
    if (b == sym_syn) {
      sequence_[write_idx--] = sym_syn_ext;
      sequence_[write_idx--] = sym_ext;
    } else if (b == sym_ext) {
      sequence_[write_idx--] = sym_ext_ext;
      sequence_[write_idx--] = sym_ext;
    } else {
      sequence_[write_idx--] = b;
    }
  }
  extended_ = true;
}

void ebus::Sequence::reduce() {
  if (!extended_) return;

  // Perform in-place unstuffing (reduced size is always <= extended size)
  size_t write_idx = 0;
  for (size_t read_idx = 0; read_idx < sequence_.size(); ++read_idx) {
    if (sequence_[read_idx] == sym_ext && read_idx + 1 < sequence_.size()) {
      uint8_t next = sequence_[++read_idx];
      if (next == sym_syn_ext)
        sequence_[write_idx++] = sym_syn;
      else if (next == sym_ext_ext)
        sequence_[write_idx++] = sym_ext;
      else {
        sequence_[write_idx++] = sym_ext;
        sequence_[write_idx++] = next;
      }
    } else {
      sequence_[write_idx++] = sequence_[read_idx];
    }
  }
  sequence_.resize(write_idx);
  extended_ = false;
}

std::string ebus::Sequence::toString() const {
  return ebus::toString(sequence_);
}

std::vector<uint8_t> ebus::Sequence::toVector() const {
  return std::vector<uint8_t>(sequence_.begin(), sequence_.end());
}
