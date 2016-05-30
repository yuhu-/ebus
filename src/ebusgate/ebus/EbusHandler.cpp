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

#include "EbusHandler.h"

EbusHandler::EbusHandler(const unsigned char address, const string device, const bool noDeviceCheck,
	const long reopenTime, const long arbitrationTime, const long receiveTimeout, const int lockCounter,
	const int lockRetries, const bool raw, const bool dump, const string dumpFile, const long dumpFileMaxSize,
	Process* process)
{
	m_ebusFSM = new EbusFSM(address, device, noDeviceCheck, reopenTime, arbitrationTime, receiveTimeout,
		lockCounter, lockRetries, raw, dump, dumpFile, dumpFileMaxSize, process);

	m_ebusFSM->start();
}

EbusHandler::~EbusHandler()
{
	if (m_ebusFSM != nullptr)
	{
		m_ebusFSM->stop();
		delete m_ebusFSM;
		m_ebusFSM = nullptr;
	}
}

void EbusHandler::open()
{
	m_ebusFSM->open();
}

void EbusHandler::close()
{
	m_ebusFSM->close();
}

bool EbusHandler::getDump() const
{
	return (m_ebusFSM->getDump());
}

void EbusHandler::setDump(bool dump)
{
	m_ebusFSM->setDump(dump);
}

bool EbusHandler::getRaw()
{
	return (m_ebusFSM->getRaw());
}

void EbusHandler::setRaw(bool raw)
{
	m_ebusFSM->setRaw(raw);
}

void EbusHandler::enqueue(EbusMessage* message)
{
	m_ebusFSM->enqueue(message);
}

