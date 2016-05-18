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
#include "MultiForward.h"
#include "DummyProcess.h"

class EbusHandler
{

public:
	EbusHandler(const unsigned char address, const string device, const bool noDeviceCheck, const long reopenTime,
		const long arbitrationTime, const long receiveTimeout, const int lockCounter, const int lockRetries,
		const bool dumpRaw, const string dumpRawFile, const long dumpRawFileMaxSize, const bool logRaw);

	~EbusHandler();

	void open();
	void close();

	bool getDumpRaw() const;
	void setDumpRaw(bool dumpRaw);

	bool getLogRaw();
	void setLogRaw(bool logRaw);

	void enqueue(EbusMessage* message);

	void forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result);

private:
	EbusHandler(const EbusHandler&);
	EbusHandler& operator=(const EbusHandler&);

	EbusFSM* m_ebusFSM = nullptr;
	MultiForward* m_multiForward = nullptr;
	DummyProcess* m_dummyProcess = nullptr;

};

#endif // EBUS_EBUSHANDLER_H
