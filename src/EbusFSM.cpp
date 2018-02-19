/*
 * Copyright (C) Roland Jax 2012-2018 <roland.jax@liwest.at>
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

#include <EbusFSM.h>
#include <EbusCommon.h>
#include <Connect.h>
#include <Idle.h>
#include <OnError.h>
#include <State.h>
#include <utils/Color.h>

#include <sstream>
#include <algorithm>

std::map<int, std::string> FSMErrors =
{
{ FSM_ERR_MASTER, "Active sending is only as master possible" },
{ FSM_ERR_SEQUENCE, "The passed sequence contains an error" },
{ FSM_ERR_ADDRESS, "The master address of the sequence and FSM must be equal" },
{ FSM_ERR_TRANSMIT, "An ebus error occurred while sending this sequence" } };

ebusfsm::EbusFSM::EbusFSM(const unsigned char address, const std::string& device, const bool deviceCheck,
	std::shared_ptr<IEbusLogger> logger, std::function<Reaction(EbusSequence&)> identify,
	std::function<void(EbusSequence&)> publish)
	: Notify(), m_address(address), m_slaveAddress(slaveAddress(address)), m_ebusDevice(
		std::make_unique<EbusDevice>(device, deviceCheck)), m_logger(logger), m_identify(identify), m_publish(publish)
{
	changeState(Connect::getConnect());

	m_thread = std::thread(&EbusFSM::run, this);
}

ebusfsm::EbusFSM::~EbusFSM()
{
	m_forceState = Idle::getIdle();

	struct timespec req =
	{ 0, 10000L };

	while (m_state != Idle::getIdle())
		nanosleep(&req, (struct timespec *) NULL);

	m_running = false;
	nanosleep(&req, (struct timespec *) NULL);

	notify();
	m_thread.join();

	while (m_ebusMsgQueue.size() > 0)
		delete m_ebusMsgQueue.dequeue();

	m_dumpRawStream.close();
}

void ebusfsm::EbusFSM::open()
{
	m_forceState = Connect::getConnect();
	notify();
}

void ebusfsm::EbusFSM::close()
{
	m_forceState = Idle::getIdle();
}

int ebusfsm::EbusFSM::transmit(EbusSequence& eSeq)
{
	int result = SEQ_OK;

	if (!isMaster(m_address))
	{
		result = FSM_ERR_MASTER;
	}
	else if (eSeq.getMasterState() != SEQ_OK)
	{
		result = FSM_ERR_SEQUENCE;
	}
	else if (eSeq.getMasterQQ() != m_address)
	{
		result = FSM_ERR_ADDRESS;
	}
	else
	{
		EbusMessage* ebusMessage = new EbusMessage(eSeq);
		m_ebusMsgQueue.enqueue(ebusMessage);
		ebusMessage->waitNotify();
		result = ebusMessage->getState();
		delete ebusMessage;
	}

	return (result);
}

const std::string ebusfsm::EbusFSM::errorText(const int error) const
{
	std::ostringstream ostr;

	if (m_color == true)
	{
		if (error < -10)
		{
			ostr << color::red << FSMErrors[error] << color::reset;
		}
		else
		{
			if (error > 0)
				ostr << color::yellow << EbusDevice::errorText(error) << color::reset;
			else
				ostr << color::red << EbusDevice::errorText(error) << color::reset;
		}
	}
	else
	{
		if (error < -10)
			ostr << FSMErrors[error];
		else
			ostr << EbusDevice::errorText(error);
	}

	return (ostr.str());
}

long ebusfsm::EbusFSM::getReopenTime() const
{
	return (m_reopenTime);
}

void ebusfsm::EbusFSM::setReopenTime(const long& reopenTime)
{
	m_reopenTime = reopenTime;
}

long ebusfsm::EbusFSM::getArbitrationTime() const
{
	return (m_arbitrationTime);
}

void ebusfsm::EbusFSM::setArbitrationTime(const long& arbitrationTime)
{
	m_arbitrationTime = arbitrationTime;
}

long ebusfsm::EbusFSM::getReceiveTimeout() const
{
	return (m_receiveTimeout);
}

void ebusfsm::EbusFSM::setReceiveTimeout(const long& receiveTimeout)
{
	m_receiveTimeout = receiveTimeout;
}

int ebusfsm::EbusFSM::getLockCounter() const
{
	return (m_lockCounter);
}

void ebusfsm::EbusFSM::setLockCounter(const int& lockCounter)
{
	m_lockCounter = lockCounter;
}

int ebusfsm::EbusFSM::getLockRetries() const
{
	return (m_lockRetries);
}

void ebusfsm::EbusFSM::setLockRetries(const int& lockRetries)
{
	m_lockRetries = lockRetries;
}

bool ebusfsm::EbusFSM::getDump() const
{
	return (m_dump);
}

void ebusfsm::EbusFSM::setDump(const bool& dump)
{
	if (dump == m_dump) return;

	m_dump = dump;

	if (dump == false)
	{
		m_dumpRawStream.close();
	}
	else
	{
		m_dumpRawStream.open(m_dumpFile.c_str(), std::ios::binary | std::ios::app);
		m_dumpFileSize = 0;
	}
}

std::string ebusfsm::EbusFSM::getDumpFile() const
{
	return (m_dumpFile);
}

void ebusfsm::EbusFSM::setDumpFile(const std::string& dumpFile)
{
	bool dump = m_dump;
	if (dump == true) setDump(false);
	m_dumpFile = dumpFile;
	m_dump = dump;
}

long ebusfsm::EbusFSM::getDumpFileMaxSize() const
{
	return (m_dumpFileMaxSize);
}

void ebusfsm::EbusFSM::setDumpFileMaxSize(const long& dumpFileMaxSize)
{
	m_dumpFileMaxSize = dumpFileMaxSize;
}

bool ebusfsm::EbusFSM::getColor() const
{
	return (m_color);
}

void ebusfsm::EbusFSM::setColor(const bool& color)
{
	m_color = color;
}

void ebusfsm::EbusFSM::run()
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

void ebusfsm::EbusFSM::changeState(State* state)
{
	if (m_state != state)
	{
		m_state = state;
		std::ostringstream ostr;

		if (m_color == true)
			ostr << color::cyan << m_state->toString() << color::reset;
		else
			ostr << m_state->toString();

		logDebug(ostr.str());
	}
}

ebusfsm::Reaction ebusfsm::EbusFSM::identify(EbusSequence& eSeq)
{
	if (m_identify != nullptr)
		return (m_identify(eSeq));
	else
		return (Reaction::nofunction);
}

void ebusfsm::EbusFSM::publish(EbusSequence& eSeq)
{
	if (m_publish != nullptr) m_publish(eSeq);
}

void ebusfsm::EbusFSM::logError(const std::string& message)
{
	if (m_logger != nullptr) m_logger->error(message);
}

void ebusfsm::EbusFSM::logWarn(const std::string& message)
{
	if (m_logger != nullptr) m_logger->warn(message);
}

void ebusfsm::EbusFSM::logInfo(const std::string& message)
{
	if (m_logger != nullptr) m_logger->info(message);
}

void ebusfsm::EbusFSM::logDebug(const std::string& message)
{
	if (m_logger != nullptr) m_logger->debug(message);
}

void ebusfsm::EbusFSM::logTrace(const std::string& message)
{
	if (m_logger != nullptr) m_logger->trace(message);
}

