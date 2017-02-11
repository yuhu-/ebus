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

#include "EbusFSM.h"
#include "EbusCommon.h"
#include "Connect.h"
#include "Idle.h"
#include "OnError.h"
#include "State.h"
#include "Color.h"

#include <sstream>
#include <algorithm>

using std::ios;
using std::pair;
using std::ostringstream;
using std::copy_n;
using std::back_inserter;

libebus::EbusFSM::EbusFSM(const unsigned char address, const string device, const bool deviceCheck,
	std::shared_ptr<IEbusProcess> process, std::shared_ptr<IEbusLogger> logger)
	: Notify(), m_address(address), m_slaveAddress(slaveAddress(address)), m_ebusDevice(
		std::make_unique<EbusDevice>(device, deviceCheck)), m_process(process), m_logger(logger)
{
	changeState(Connect::getConnect());
}

libebus::EbusFSM::~EbusFSM()
{
	m_dumpRawStream.close();
}

void libebus::EbusFSM::start()
{
	m_thread = thread(&EbusFSM::run, this);
}

void libebus::EbusFSM::stop()
{
	m_forceState = Idle::getIdle();
	notify();
	m_running = false;
	m_thread.join();
}

void libebus::EbusFSM::open()
{
	m_forceState = Connect::getConnect();
	notify();
}

void libebus::EbusFSM::close()
{
	m_forceState = Idle::getIdle();
}

long libebus::EbusFSM::getReopenTime() const
{
	return (m_reopenTime);
}

void libebus::EbusFSM::setReopenTime(const long& reopenTime)
{
	m_reopenTime = reopenTime;
}

long libebus::EbusFSM::getArbitrationTime() const
{
	return (m_arbitrationTime);
}

void libebus::EbusFSM::setArbitrationTime(const long& arbitrationTime)
{
	m_arbitrationTime = arbitrationTime;
}

long libebus::EbusFSM::getReceiveTimeout() const
{
	return (m_receiveTimeout);
}

void libebus::EbusFSM::setReceiveTimeout(const long& receiveTimeout)
{
	m_receiveTimeout = receiveTimeout;
}

int libebus::EbusFSM::getLockCounter() const
{
	return (m_lockCounter);
}

void libebus::EbusFSM::setLockCounter(const int& lockCounter)
{
	m_lockCounter = lockCounter;
}

int libebus::EbusFSM::getLockRetries() const
{
	return (m_lockRetries);
}

void libebus::EbusFSM::setLockRetries(const int& lockRetries)
{
	m_lockRetries = lockRetries;
}

bool libebus::EbusFSM::getDump() const
{
	return (m_dump);
}

void libebus::EbusFSM::setDump(const bool& dump)
{
	if (dump == m_dump) return;

	m_dump = dump;

	if (dump == false)
	{
		m_dumpRawStream.close();
	}
	else
	{
		m_dumpRawStream.open(m_dumpFile.c_str(), ios::binary | ios::app);
		m_dumpFileSize = 0;
	}
}

string libebus::EbusFSM::getDumpFile() const
{
	return (m_dumpFile);
}

void libebus::EbusFSM::setDumpFile(const string& dumpFile)
{
	bool dump = m_dump;
	if (dump == true) setDump(false);
	m_dumpFile = dumpFile;
	m_dump = dump;
}

long libebus::EbusFSM::getDumpFileMaxSize() const
{
	return (m_dumpFileMaxSize);
}

void libebus::EbusFSM::setDumpFileMaxSize(const long& dumpFileMaxSize)
{
	m_dumpFileMaxSize = dumpFileMaxSize;
}

void libebus::EbusFSM::run()
{
	logInfo("EbusFSM started");

	while (m_running == true)
	{
		m_lastResult = m_state->run(this);
		if (m_lastResult != DEV_OK) changeState(OnError::getOnError());

		if (m_forceState != nullptr)
		{
			changeState(m_forceState);
			m_forceState = nullptr;
		}
	}

	logInfo("EbusFSM stopped");
}

void libebus::EbusFSM::changeState(State* state)
{
	if (m_state != state)
	{
		m_state = state;

		ostringstream ostr;
		ostr << libutils::color::cyan << m_state->toString() << libutils::color::reset;
		logDebug(ostr.str());
	}
}

libebus::Action libebus::EbusFSM::getEvaluatedAction(EbusSequence& eSeq)
{
	if (m_process != nullptr)
		return (m_process->getEvaluatedAction(eSeq));
	else
		return (Action::noprocess);
}

void libebus::EbusFSM::evalActiveMessage(EbusSequence& eSeq)
{
	if (m_process != nullptr) m_process->evalActiveMessage(eSeq);
}

void libebus::EbusFSM::evalPassiveMessage(EbusSequence& eSeq)
{
	if (m_process != nullptr) m_process->evalPassiveMessage(eSeq);
}

libebus::EbusMessage* libebus::EbusFSM::dequeueMessage()
{
	if (m_process != nullptr)
		return (m_process->dequeueMessage());
	else
		return (nullptr);
}

size_t libebus::EbusFSM::getQueueSize()
{
	if (m_process != nullptr)
		return (m_process->getQueueSize());
	else
		return (0);
}

void libebus::EbusFSM::logError(const string& message)
{
	if (m_logger != nullptr) m_logger->error(message);
}

void libebus::EbusFSM::logWarn(const string& message)
{
	if (m_logger != nullptr) m_logger->warn(message);
}

void libebus::EbusFSM::logInfo(const string& message)
{
	if (m_logger != nullptr) m_logger->info(message);
}

void libebus::EbusFSM::logDebug(const string& message)
{
	if (m_logger != nullptr) m_logger->debug(message);
}

void libebus::EbusFSM::logTrace(const string& message)
{
	if (m_logger != nullptr) m_logger->trace(message);
}

