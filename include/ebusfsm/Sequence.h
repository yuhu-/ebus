/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
 *
 * This file is part of ebusfsm.
 *
 * ebusfsm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusfsm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusfsm. If not, see http://www.gnu.org/licenses/.
 */

#ifndef EBUSFSM_SEQUENCE_H
#define EBUSFSM_SEQUENCE_H

#include <vector>
#include <string>
#include <cstddef>

namespace ebusfsm
{

static std::byte seq_zero;    // 0x00

static std::byte seq_syn;     // 0xaa synchronization byte
static std::byte seq_exp;     // 0xa9 expand byte
static std::byte seq_synexp;  // 0x01 expanded synchronization byte
static std::byte seq_expexp;  // 0x00 expanded expand byte

class Sequence
{

public:
	static const size_t npos = -1;

	Sequence();
	explicit Sequence(const std::string& str);
	Sequence(const Sequence& seq, const size_t index, size_t len = 0);

	void push_back(const std::byte byte, const bool isExtended = true);

	const std::byte& operator[](const size_t index) const;
	std::vector<std::byte> range(const size_t index, const size_t len);

	size_t size() const;

	void clear();

	std::byte getCRC();

	void extend();
	void reduce();

	bool isExtended() const;

	const std::string toString() const;
	const std::vector<std::byte> getSequence() const;

	size_t find(const Sequence& seq, const size_t pos = 0) const noexcept;

	int compare(const Sequence& seq) const noexcept;

	bool contains(const std::string& str) const noexcept;

	static const std::string toString(const std::vector<std::byte>& seq);

private:
	std::vector<std::byte> m_seq;

	bool m_extended = false;

};

} // namespace ebusfsm

#endif // EBUSFSM_SEQUENCE_H

