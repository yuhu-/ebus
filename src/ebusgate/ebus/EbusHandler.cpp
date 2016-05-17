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
	const int lockRetries, const bool dumpRaw, const string dumpRawFile, const long dumpRawFileMaxSize,
	const bool logRaw)
{
	m_multiForward = new MultiForward();
	m_multiForward->start();

	m_dummyProcess = new DummyProcess(address);
	m_dummyProcess->start();

	m_ebusFSM = new EbusFSM(address, device, noDeviceCheck, reopenTime, arbitrationTime, receiveTimeout,
		lockCounter, lockRetries, dumpRaw, dumpRawFile, dumpRawFileMaxSize, logRaw, m_multiForward,
		m_dummyProcess);

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

	if (m_dummyProcess != nullptr)
	{
		m_dummyProcess->stop();
		delete m_dummyProcess;
		m_dummyProcess = nullptr;
	}

	if (m_multiForward != nullptr)
	{
		m_multiForward->stop();
		delete m_multiForward;
		m_multiForward = nullptr;
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

bool EbusHandler::getDumpRaw() const
{
	return (m_ebusFSM->getDumpRaw());
}

void EbusHandler::setDumpRaw(bool dumpRaw)
{
	m_ebusFSM->setDumpRaw(dumpRaw);
}

bool EbusHandler::getLogRaw()
{
	return (m_ebusFSM->getLogRaw());
}

void EbusHandler::setLogRaw(bool logRaw)
{
	m_ebusFSM->setLogRaw(logRaw);
}

void EbusHandler::enqueue(EbusMessage* message)
{
	m_ebusFSM->enqueue(message);
}

void EbusHandler::forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result)
{
	if (remove == true)
		m_multiForward->remove(ip, port, filter, result);
	else
		m_multiForward->append(ip, port, filter, result);
}


