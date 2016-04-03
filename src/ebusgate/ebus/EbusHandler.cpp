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
#include "State.h"
#include "Idle.h"
#include "Connect.h"
#include "OnError.h"
#include "Logger.h"

#include <sstream>
#include <algorithm>

using std::ios;
using std::pair;
using std::ostringstream;
using std::copy_n;
using std::back_inserter;

EbusHandler::EbusHandler(const unsigned char address, const string device, const bool noDeviceCheck,
	const long reopenTime, const long arbitrationTime, const long receiveTimeout, const int lockCounter,
	const int lockRetries, const bool dumpRaw, const string dumpRawFile, const long dumpRawFileMaxSize,
	const bool logRaw)
	: m_address(address), m_reopenTime(reopenTime), m_arbitrationTime(arbitrationTime), m_receiveTimeout(
		receiveTimeout), m_lockCounter(lockCounter), m_lockRetries(lockRetries), m_lastResult(
	DEV_OK), m_dumpRawFile(dumpRawFile), m_dumpRawFileMaxSize(dumpRawFileMaxSize), m_logRaw(logRaw)
{
	m_dataHandler = new DataHandler();
	m_dataHandler->start();

	m_device = new EbusDevice(device, noDeviceCheck);
	changeState(Connect::getConnect());

	setDumpRaw(dumpRaw);
}

EbusHandler::~EbusHandler()
{
	if (m_device != nullptr)
	{
		delete m_device;
		m_device = nullptr;
	}

	m_dumpRawStream.close();

	while (m_rules.size() > 0)
	{
		Rule* rule = m_rules.back();
		m_rules.pop_back();
		delete rule;
	}

	if (m_dataHandler != nullptr)
	{
		m_dataHandler->stop();
		delete m_dataHandler;
		m_dataHandler = nullptr;
	}
}

void EbusHandler::start()
{
	m_thread = thread(&EbusHandler::run, this);
}

void EbusHandler::stop()
{
	m_forceState = Idle::getIdle();
	notify();
	m_running = false;
	m_thread.join();
}

void EbusHandler::open()
{
	m_forceState = Connect::getConnect();
	notify();
}

void EbusHandler::close()
{
	m_forceState = Idle::getIdle();
}

bool EbusHandler::getDumpRaw() const
{
	return (m_dumpRaw);
}

void EbusHandler::setDumpRaw(bool dumpRaw)
{
	if (dumpRaw == m_dumpRaw) return;

	m_dumpRaw = dumpRaw;

	if (dumpRaw == false)
	{
		m_dumpRawStream.close();
	}
	else
	{
		m_dumpRawStream.open(m_dumpRawFile.c_str(), ios::binary | ios::app);
		m_dumpRawFileSize = 0;
	}
}

bool EbusHandler::getLogRaw()
{
	return (m_logRaw);
}

void EbusHandler::setLogRaw(bool logRaw)
{
	m_logRaw = logRaw;
}

void EbusHandler::enqueue(EbusMessage* message)
{
	m_ebusMsgQueue.enqueue(message);
}

bool EbusHandler::forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result)
{
	if (remove == true)
		return (m_dataHandler->remove(ip, port, filter, result));
	else
		return (m_dataHandler->append(ip, port, filter, result));
}

bool EbusHandler::process(bool remove, const string& filter, const string& rule, const string& message,
	ostringstream& result)
{
	if (remove == true)
		return (this->remove(filter, result));
	else
		return (this->append(filter, rule, message, result));
}

void EbusHandler::run()
{
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
}

void EbusHandler::changeState(State* state)
{
	Logger logger = Logger("EbusHandler::changeState");

	if (m_state != state)
	{
		m_state = state;
		logger.trace("%s", m_state->toString().c_str());
	}
}

bool EbusHandler::append(const string& filter, const string& rule, const string& message, ostringstream& result)
{
	Logger logger = Logger("EbusHandler::append");

	RuleType type = findType(rule);
	if (type == rt_undefined)
	{
		result << "type " << rule << " is not defined";
		logger.warn("%s", result.str().c_str());
		return (false);
	}

	if (type > rt_ignore && message.empty() == true)
	{
		result << "message is missing";
		logger.info("%s", result.str().c_str());
		return (false);
	}

	if (getRule(filter) == nullptr)
	{
		addRule(filter, type, message);
		result << "rule for " << filter << " added";
	}
	else
	{
		if (delRule(filter) == true)
		{
			addRule(filter, type, message);
			result << "rule for " << filter << " updated";
		}
		else
		{
			result << "update rule for " << filter << " failed";
			logger.warn("%s", result.str().c_str());
			return (false);
		}

	}

	logger.info("%s", result.str().c_str());
	return (true);
}

bool EbusHandler::remove(const string& filter, ostringstream& result)
{
	Logger logger = Logger("EbusHandler::remove");

	if (delRule(filter) == true)
	{
		result << "rule for " << filter << " removed";
		return (true);
	}
	else
	{
		result << "remove rule for " << filter << " failed";
		logger.warn("%s", result.str().c_str());
		return (false);
	}

}

const Rule* EbusHandler::getRule(const string& filter) const
{
	Sequence seq(filter);

	for (size_t index = 0; index < m_rules.size(); index++)
		if (m_rules[index]->equal(seq) == true) return (m_rules[index]);

	return (nullptr);
}

const Rule* EbusHandler::addRule(const string& filter, RuleType type, const string& message)
{
	Sequence seq(filter);
	size_t index;

	for (index = 0; index < m_rules.size(); index++)
		if (m_rules[index]->equal(seq) == true) break;

	if (index == m_rules.size()) m_rules.push_back(new Rule(seq, type, message));

	return (m_rules[index]);
}

bool EbusHandler::delRule(const string& filter)
{
	Sequence seq(filter);

	for (size_t index = 0; index < m_rules.size(); index++)
		if (m_rules[index]->equal(seq) == true)
		{
			Rule* rule = m_rules[index];

			m_rules.erase(m_rules.begin() + index);
			m_rules.shrink_to_fit();

			delete rule;
			return (true);
		}

	return (false);
}

RuleType EbusHandler::getType(const EbusSequence& eSeq) const
{
	for (const auto& rule : m_rules)
		if (rule->match(eSeq.getMaster()) == true) return (rule->getType());

	return (rt_undefined);
}

bool EbusHandler::createResponse(EbusSequence& eSeq)
{
	for (const auto& rule : m_rules)
		if (rule->match(eSeq.getMaster()) == true)
		{
			Sequence seq(rule->getMessage());
			eSeq.createSlave(seq);
			if (eSeq.getSlaveState() == EBUS_OK) return (true);
			break;
		}

	return (false);
}

bool EbusHandler::createMessage(const unsigned char target, EbusSequence& eSeq)
{
	for (const auto& rule : m_rules)
		if (rule->match(eSeq.getMaster()) == true)
		{
			eSeq.clear();
			eSeq.createMaster(m_address, target, rule->getMessage());
			if (eSeq.getMasterState() == EBUS_OK) return (true);
			break;
		}

	return (false);
}

