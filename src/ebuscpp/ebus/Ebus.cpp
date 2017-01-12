/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
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

#include "Ebus.h"

Ebus::Ebus(const unsigned char address, const string device, const bool devicecheck, IProcess* process,
	const long reopenTime, const long arbitrationTime, const long receiveTimeout, const int lockCounter,
	const int lockRetries, const bool dump, const string dumpFile, const long dumpFileMaxSize)
{
	m_ebusFSM = new EbusFSM(address, device, devicecheck, process, &m_logger, reopenTime, arbitrationTime,
		receiveTimeout, lockCounter, lockRetries, dump, dumpFile, dumpFileMaxSize);

	m_ebusFSM->start();
}

Ebus::~Ebus()
{
	if (m_ebusFSM != nullptr)
	{
		m_ebusFSM->stop();
		delete m_ebusFSM;
		m_ebusFSM = nullptr;
	}
}

void Ebus::open()
{
	m_ebusFSM->open();
}

void Ebus::close()
{
	m_ebusFSM->close();
}

bool Ebus::getDump() const
{
	return (m_ebusFSM->getDump());
}

void Ebus::setDump(bool dump)
{
	m_ebusFSM->setDump(dump);
}

void Ebus::enqueue(EbusMessage* message)
{
	m_ebusFSM->enqueue(message);
}

