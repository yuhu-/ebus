/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#ifndef EBUS_SEQUENCE_H
#define EBUS_SEQUENCE_H

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace ebus
{

static const std::byte seq_zero = std::byte(0x00);     // zero byte

static const std::byte seq_syn = std::byte(0xaa);      // synchronization byte
static const std::byte seq_exp = std::byte(0xa9);      // expand byte
static const std::byte seq_synexp = std::byte(0x01);   // expanded synchronization byte
static const std::byte seq_expexp = std::byte(0x00);   // expanded expand byte

class Sequence
{

public:
	static const size_t npos = -1;

	Sequence();
	explicit Sequence(const std::string &str);
	Sequence(const Sequence &seq, const size_t index, size_t len = 0);

	void push_back(const std::byte byte, const bool isExtended = true);

	const std::byte& operator[](const size_t index) const;
	const std::vector<std::byte> range(const size_t index, const size_t len);

	size_t size() const;

	void clear();

	std::byte getCRC();

	void extend();
	void reduce();

	bool isExtended() const;

	const std::string toString() const;
	const std::vector<std::byte> getSequence() const;

	size_t find(const Sequence &seq, const size_t pos = 0) const noexcept;

	int compare(const Sequence &seq) const noexcept;

	bool contains(const std::string &str) const noexcept;

	static const std::vector<std::byte> toVector(const std::string &str);

	static const std::string toString(const std::vector<std::byte> &seq);

	static bool isHex(const std::string &str, std::ostringstream &result, const int &nibbles);

private:
	std::vector<std::byte> m_seq;

	bool m_extended = false;

	static std::byte calcCRC(const std::byte byte, const std::byte init);
};

} // namespace ebus

#endif // EBUS_SEQUENCE_H

