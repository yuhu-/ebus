/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
 *
 * This file is part of ebuscpp.
 *
 * ebuscpp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebuscpp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebuscpp. If not, see http://www.gnu.org/licenses/.
 */

#ifndef EBUS_EBUS_H
#define EBUS_EBUS_H

#include "EbusFSM.h"

class Ebus
{

public:
	Ebus(const unsigned char address, const string device, const bool noDeviceCheck, const long reopenTime,
		const long arbitrationTime, const long receiveTimeout, const int lockCounter, const int lockRetries,
		const bool raw, const bool dump, const string dumpFile, const long dumpFileMaxSize, IProcess* process);

	~Ebus();

	void open();
	void close();

	bool getDump() const;
	void setDump(bool dump);

	bool getRaw() const;
	void setRaw(bool raw);

	void enqueue(EbusMessage* message);

private:
	Ebus(const Ebus&);
	Ebus& operator=(const Ebus&);

	EbusFSM* m_ebusFSM = nullptr;

};

#endif // EBUS_EBUS_H
