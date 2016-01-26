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

#ifndef EBUS_EBUSDATASTORE_H
#define EBUS_EBUSDATASTORE_H

#include "EbusSequence.h"

#include <map>
#include <vector>

using std::map;
using std::vector;

class EbusDataStore
{

public:
	~EbusDataStore();

	void write(const EbusSequence& eSeq);

	const string read(const string& str);

private:
	map<vector<unsigned char>, EbusSequence> m_eSeqStore;

};

#endif // EBUS_EBUSDATASTORE_H
