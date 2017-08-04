/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
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

namespace ebusfsm
{

#define SYN       0xaa  // synchronization byte
#define SYNEXT    0x01  // extended synchronization byte
#define EXT       0xa9  // extend byte
#define EXTEXT    0x00  // extended extend byte

class Sequence
{
	friend class EbusSequence;

public:
	static const size_t npos = -1;

	Sequence();
	explicit Sequence(const std::string& str);
	Sequence(const Sequence& seq, const size_t index, size_t len = 0);

	void push_back(const unsigned char byte, const bool isExtended = true);

	const unsigned char& operator[](const size_t index) const;

	size_t size() const;

	void clear();

	unsigned char getCRC();

	void extend();
	void reduce();

	bool isExtended() const;

	const std::string toString();
	const std::vector<unsigned char> getSequence() const;

	size_t find(const Sequence& seq, const size_t pos = 0) const noexcept;

	int compare(const Sequence& seq) const noexcept;

	bool contains(const std::string& str) const noexcept;

	static const std::string toString(const std::vector<unsigned char>& seq);

private:
	std::vector<unsigned char> m_seq;

	bool m_extended = false;

};

} // namespace ebusfsm

#endif // EBUSFSM_SEQUENCE_H

