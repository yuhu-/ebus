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

#include "EbusDataStore.h"
#include "Logger.h"

#include <iomanip>
#include <algorithm>

using std::pair;
using std::ostringstream;
using std::copy_n;
using std::back_inserter;

extern Logger& L;

EbusDataStore::~EbusDataStore()
{
	m_eSeqStore.clear();
}

void EbusDataStore::write(const EbusSequence& eSeq)
{
	vector<unsigned char> key;
	int size = eSeq.getMasterNN();

	if (size > 3) size = 3;

	size += 5;

	copy_n(eSeq.getMaster().getSequence().begin(), size,
		back_inserter(key));

	map<vector<unsigned char>, EbusSequence>::iterator it =
		m_eSeqStore.find(key);

	if (it != m_eSeqStore.end())
	{
		it->second = eSeq;
		L.log(debug, "%03d - update key %s", m_eSeqStore.size(),
			Sequence::toString(key).c_str());
	}
	else
	{
		m_eSeqStore.insert(
			pair<vector<unsigned char>, EbusSequence>(key, eSeq));
		L.log(debug, "%03d - insert key %s", m_eSeqStore.size(),
			Sequence::toString(key).c_str());
	}
}

const string EbusDataStore::read(const string& str)
{
	ostringstream result;
	const Sequence seq(str);
	vector<unsigned char> key(seq.getSequence());

	map<vector<unsigned char>, EbusSequence>::iterator it =
		m_eSeqStore.find(key);
	if (it != m_eSeqStore.end())
	{
		result << it->second.toString();
		L.log(debug, "key %s found %s", Sequence::toString(key).c_str(),
			result.str().c_str());
	}
	else
	{
		result << "not found";
		L.log(debug, "key %s not found",
			Sequence::toString(key).c_str());
	}

	return (result.str());
}

