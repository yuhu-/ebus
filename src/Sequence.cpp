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

#include <Sequence.h>
#include <EbusCommon.h>

#include <sstream>
#include <iomanip>

ebusfsm::Sequence::Sequence()
{
}

ebusfsm::Sequence::Sequence(const std::string& str)
{
	for (size_t i = 0; i + 1 < str.size(); i += 2)
	{
		unsigned long byte = std::strtoul(str.substr(i, 2).c_str(), nullptr, 16);
		push_back((unsigned char) byte, false);
	}
}

ebusfsm::Sequence::Sequence(const Sequence& seq, const size_t index, size_t len)
{
	if (len == 0) len = seq.size() - index;

	for (size_t i = index; i < index + len; i++)
		m_seq.push_back(seq.m_seq.at(i));

	m_extended = seq.m_extended;
}

void ebusfsm::Sequence::push_back(const unsigned char byte, const bool isExtended)
{
	m_seq.push_back(byte);
	m_extended = isExtended;
}

const unsigned char& ebusfsm::Sequence::operator[](const size_t index) const
{
	return (m_seq.at(index));
}

std::vector<unsigned char> ebusfsm::Sequence::range(const size_t index, const size_t len)
{
	std::vector<unsigned char> result;

	for (size_t i = index; i < m_seq.size() && result.size() < len; i++)
		result.push_back(m_seq.at(i));

	return (result);
}

size_t ebusfsm::Sequence::size() const
{
	return (m_seq.size());
}

void ebusfsm::Sequence::clear()
{
	m_seq.clear();
	m_seq.shrink_to_fit();
	m_extended = false;
}

unsigned char ebusfsm::Sequence::getCRC()
{
	if (m_extended == false) extend();

	unsigned char crc = 0;

	for (size_t i = 0; i < m_seq.size(); i++)
		crc = calcCRC(m_seq.at(i), crc);

	if (m_extended == false) reduce();

	return (crc);
}

void ebusfsm::Sequence::extend()
{
	if (m_extended == true) return;

	std::vector<unsigned char> tmp;

	for (size_t i = 0; i < m_seq.size(); i++)
	{
		if (m_seq.at(i) == SYN)
		{
			tmp.push_back(EXT);
			tmp.push_back(SYNEXT);
		}
		else if (m_seq.at(i) == EXT)
		{
			tmp.push_back(EXT);
			tmp.push_back(EXTEXT);
		}
		else
		{
			tmp.push_back(m_seq.at(i));
		}
	}

	m_seq = tmp;
	m_extended = true;
}

void ebusfsm::Sequence::reduce()
{
	if (m_extended == false) return;

	std::vector<unsigned char> tmp;
	bool extended = false;

	for (size_t i = 0; i < m_seq.size(); i++)
	{
		if (m_seq.at(i) == SYN || m_seq.at(i) == EXT)
		{
			extended = true;
		}
		else if (extended == true)
		{
			if (m_seq.at(i) == SYNEXT)
				tmp.push_back(SYN);
			else
				tmp.push_back(EXT);

			extended = false;
		}
		else
		{
			tmp.push_back(m_seq.at(i));
		}
	}

	m_seq = tmp;
	m_extended = false;
}

bool ebusfsm::Sequence::isExtended() const
{
	return (m_extended);
}

const std::string ebusfsm::Sequence::toString() const
{
	std::ostringstream ostr;

	for (size_t i = 0; i < m_seq.size(); i++)
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(m_seq.at(i));

	return (ostr.str());
}

const std::vector<unsigned char> ebusfsm::Sequence::getSequence() const
{
	return (m_seq);
}

size_t ebusfsm::Sequence::find(const Sequence& seq, const size_t pos) const noexcept
{
	for (size_t i = pos; i + seq.size() <= m_seq.size(); i++)
		if (equal(m_seq.begin() + i, m_seq.begin() + i + seq.size(), seq.m_seq.begin()) == true) return (i);

	return (npos);
}

int ebusfsm::Sequence::compare(const Sequence& seq) const noexcept
{
	if (m_seq.size() < seq.size())
		return (-1);
	else if (m_seq.size() > seq.size())
		return (1);
	else if (equal(m_seq.begin(), m_seq.end(), seq.m_seq.begin()) == true) return (0);

	return (-1);
}

bool ebusfsm::Sequence::contains(const std::string& str) const noexcept
{
	if (find(Sequence(str)) != npos) return (true);

	return (false);
}

const std::string ebusfsm::Sequence::toString(const std::vector<unsigned char>& seq)
{
	std::ostringstream ostr;

	for (size_t i = 0; i < seq.size(); i++)
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(seq[i]);

	return (ostr.str());
}

