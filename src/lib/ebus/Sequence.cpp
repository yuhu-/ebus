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
using std::equal;

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

Sequence::Sequence(const Sequence& seq, const size_t index, size_t len)
{
	if (len == 0) len = seq.size() - index;

	for (size_t i = index; i < index + len; i++)
		m_seq.push_back(seq.m_seq[i]);

	m_extended = seq.m_extended;
}

void Sequence::push_back(const unsigned char byte, const bool isExtended)
{
	m_seq.push_back(byte);
	m_extended = isExtended;
}

const unsigned char& Sequence::operator[](const size_t index) const
{
	return (m_seq[index]);
}

size_t Sequence::size() const
{
	return (m_seq.size());
}

void Sequence::clear()
{
	m_seq.clear();
	m_extended = false;
}

unsigned char Sequence::getCRC()
{
	if (m_extended == false) extend();

	unsigned char crc = 0;

	for (size_t i = 0; i < m_seq.size(); i++)
		crc = calcCRC(m_seq[i], crc);

	if (m_extended == false) reduce();

	return (crc);
}

void Sequence::extend()
{
	if (m_extended == true) return;

	vector<unsigned char> tmp;

	for (size_t i = 0; i < m_seq.size(); i++)
	{
		if (m_seq[i] == SYN)
		{
			tmp.push_back(EXT);
			tmp.push_back(SYNEXT);
		}
		else if (m_seq[i] == EXT)
		{
			tmp.push_back(EXT);
			tmp.push_back(EXTEXT);
		}
		else
		{
			tmp.push_back(m_seq[i]);
		}
	}

	m_seq = tmp;
	m_extended = true;
}

void Sequence::reduce()
{
	if (m_extended == false) return;

	vector<unsigned char> tmp;
	bool extended = false;

	for (size_t i = 0; i < m_seq.size(); i++)
	{
		if (m_seq[i] == SYN || m_seq[i] == EXT)
		{
			extended = true;
		}
		else if (extended == true)
		{
			if (m_seq[i] == SYNEXT)
				tmp.push_back(SYN);
			else
				tmp.push_back(EXT);

			extended = false;
		}
		else
		{
			tmp.push_back(m_seq[i]);
		}
	}

	m_seq = tmp;
	m_extended = false;
}

bool Sequence::isExtended() const
{
	return (m_extended);
}

const string Sequence::toString()
{
	ostringstream ostr;

	for (size_t i = 0; i < m_seq.size(); i++)
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(m_seq[i]);

	return (ostr.str());
}

const vector<unsigned char> Sequence::getSequence() const
{
	return (m_seq);
}

size_t Sequence::find(const Sequence& seq, const size_t pos) const noexcept
{
	for (size_t i = pos; i + seq.size() <= m_seq.size(); i++)
		if (equal(m_seq.begin() + i, m_seq.begin() + i + seq.size(), seq.m_seq.begin()) == true) return (i);

	return (npos);
}

int Sequence::compare(const Sequence& seq) const noexcept
{
	if (m_seq.size() < seq.size())
		return (-1);
	else if (m_seq.size() > seq.size())
		return (1);
	else if (equal(m_seq.begin(), m_seq.end(), seq.m_seq.begin()) == true) return (0);

	return (-1);
}

const string Sequence::toString(const vector<unsigned char>& seq)
{
	ostringstream ostr;

	for (size_t i = 0; i < seq.size(); i++)
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(seq[i]);

	return (ostr.str());
}

