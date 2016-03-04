/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
 *
 * This file is part of ebusgate.
 *
 * ebusgate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusgate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusgate. If not, see http://www.gnu.org/licenses/.
 */

#ifndef LIBEBUS_SEQUENCE_H
#define LIBEBUS_SEQUENCE_H

#include <vector>
#include <string>

using std::string;
using std::vector;

#define SYN       0xaa  // synchronization byte
#define SYNEXT    0x01  // extended synchronization byte
#define EXT       0xa9  // extend byte
#define EXTEXT    0x00  // extended extend byte

class Sequence
{
	friend class EbusSequence;

public:
	Sequence();
	explicit Sequence(const string& str);
	Sequence(const Sequence& seq, const size_t index, size_t len = 0);

	void push_back(const unsigned char& byte, const bool isExtended = true);

	const unsigned char& operator[](const size_t index) const;

	size_t size() const;

	void clear();

	unsigned char getCRC();

	void extend();
	void reduce();

	bool isExtended() const;

	const string toString();
	const vector<unsigned char> getSequence() const;

	bool compare(const Sequence& seq, const size_t& pos = 0);

	int search(const Sequence& seq);

	static const string toString(const vector<unsigned char>& sequence);

private:
	vector<unsigned char> m_sequence;

	bool m_extended = false;

};

#endif // LIBEBUS_SEQUENCE_H

