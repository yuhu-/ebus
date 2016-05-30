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

#ifndef EBUS_EBUSHANDLER_H
#define EBUS_EBUSHANDLER_H

#include "EbusFSM.h"

class EbusHandler
{

public:
	EbusHandler(const unsigned char address, const string device, const bool noDeviceCheck, const long reopenTime,
		const long arbitrationTime, const long receiveTimeout, const int lockCounter, const int lockRetries,
		const bool raw, const bool dump, const string dumpFile, const long dumpFileMaxSize, Process* process);

	~EbusHandler();

	void open();
	void close();

	bool getDump() const;
	void setDump(bool dump);

	bool getRaw();
	void setRaw(bool raw);

	void enqueue(EbusMessage* message);

private:
	EbusHandler(const EbusHandler&);
	EbusHandler& operator=(const EbusHandler&);

	EbusFSM* m_ebusFSM = nullptr;

};

#endif // EBUS_EBUSHANDLER_H
