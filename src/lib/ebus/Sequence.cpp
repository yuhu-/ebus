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

#include "Sequence.h"
#include "Common.h"

#include <sstream>
#include <iomanip>

using std::ostringstream;
using std::nouppercase;
using std::hex;
using std::setw;
using std::setfill;

Sequence::Sequence()
{
}

Sequence::Sequence(const string& str)
{
	for (size_t i = 0; i + 1 < str.size(); i += 2)
	{
		unsigned long byte = strtoul(str.substr(i, 2).c_str(), nullptr, 16);
		push_back((unsigned char) byte, false);
	}
}

Sequence::Sequence(const Sequence& seq)
{
	Sequence(seq, 0);
}

Sequence::Sequence(const Sequence& seq, const size_t index, size_t len)
{
	if (len == 0) len = seq.size() - index;

	for (size_t i = index; i < index + len; i++)
		m_sequence.push_back(seq.m_sequence[i]);

	m_extended = seq.m_extended;
}

void Sequence::push_back(const unsigned char& byte, const bool isExtended)
{
	m_sequence.push_back(byte);
	m_extended = isExtended;
}

const unsigned char& Sequence::operator[](const size_t index) const
{
	return (m_sequence[index]);
}

size_t Sequence::size() const
{
	return (m_sequence.size());
}

void Sequence::clear()
{
	m_sequence.clear();
	m_extended = false;
}

unsigned char Sequence::getCRC()
{
	if (m_extended == false) extend();

	unsigned char crc = 0;

	for (size_t i = 0; i < m_sequence.size(); i++)
		crc = calcCRC(m_sequence[i], crc);

	if (m_extended == false) reduce();

	return (crc);
}

void Sequence::extend()
{
	if (m_extended == true) return;

	vector<unsigned char> tmp;

	for (size_t i = 0; i < m_sequence.size(); i++)
	{
		if (m_sequence[i] == SYN)
		{
			tmp.push_back(EXT);
			tmp.push_back(SYNEXT);
		}
		else if (m_sequence[i] == EXT)
		{
			tmp.push_back(EXT);
			tmp.push_back(EXTEXT);
		}
		else
		{
			tmp.push_back(m_sequence[i]);
		}
	}

	m_sequence = tmp;
	m_extended = true;
}

void Sequence::reduce()
{
	if (m_extended == false) return;

	vector<unsigned char> tmp;
	bool extended = false;

	for (size_t i = 0; i < m_sequence.size(); i++)
	{
		if (m_sequence[i] == SYN || m_sequence[i] == EXT)
		{
			extended = true;
		}
		else if (extended == true)
		{
			if (m_sequence[i] == SYNEXT)
				tmp.push_back(SYN);
			else
				tmp.push_back(EXT);

			extended = false;
		}
		else
		{
			tmp.push_back(m_sequence[i]);
		}
	}

	m_sequence = tmp;
	m_extended = false;
}

bool Sequence::isExtended() const
{
	return (m_extended);
}

const string Sequence::toString()
{
	ostringstream ostr;

	for (size_t i = 0; i < m_sequence.size(); i++)
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_sequence[i]);

	return (ostr.str());
}

const vector<unsigned char> Sequence::getSequence() const
{
	return (m_sequence);
}

const string Sequence::toString(const vector<unsigned char>& sequence)
{
	ostringstream ostr;

	for (size_t i = 0; i < sequence.size(); i++)
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(sequence[i]);

	return (ostr.str());
}

