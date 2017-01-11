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

#include <LogHandler.h>
#include "LogConsole.h"
#include "LogFile.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <cstring>

using std::istringstream;
using std::ostringstream;
using std::endl;
using std::setw;
using std::setfill;
using std::left;

liblogger::LogHandler& liblogger::LogHandler::getLogHandler()
{
	static LogHandler logHandler;
	return (logHandler);
}

liblogger::LogHandler::~LogHandler()
{
	while (m_sinks.size() > 0)
		delSink(*(m_sinks.begin()));

	if (m_thread.joinable())
	{
		m_logMessages.enqueue(nullptr);
		m_thread.join();
	}
}

void liblogger::LogHandler::start()
{
	m_thread = thread(&LogHandler::run, this);
}

liblogger::Level liblogger::LogHandler::getLevel() const
{
       return (m_level);
}

string liblogger::LogHandler::getLevelName(Level level)
{
	return (LevelNames[level]);
}

void liblogger::LogHandler::setLevel(const string& level)
{
	m_level = findLevel(level);
}

void liblogger::LogHandler::addConsole()
{
	addSink(new LogConsole());
}

void liblogger::LogHandler::addFile(const string& file)
{
	addSink(new LogFile(file));
}

void liblogger::LogHandler::log(const LogMessage* logMessage)
{
	m_logMessages.enqueue(logMessage);
}

liblogger::LogHandler::LogHandler()
{
}

void liblogger::LogHandler::run()
{
	while (true)
	{
		const LogMessage* message = m_logMessages.dequeue();
		if (message == nullptr) break;

		ostringstream ostr;

		ostr << "[" << message->getTime() << "] " << setw(5) << setfill(' ') << left << message->getLevel()
			<< " " << message->getFunction() << " " << message->getText() << endl;

		for (const auto& sink : m_sinks)
			sink->write(ostr.str());

		delete message;
	}
}

liblogger::Level liblogger::LogHandler::findLevel(const string& level)
{
	for (const auto& lvl : LevelNames)
		if (strcasecmp(lvl.second.c_str(), level.c_str()) == 0) return (lvl.first);

	return (Level::info);
}

void liblogger::LogHandler::addSink(LogSink* sink)
{
	vector<LogSink*>::iterator it = find(m_sinks.begin(), m_sinks.end(), sink);

	if (it == m_sinks.end()) m_sinks.push_back(sink);

}

void liblogger::LogHandler::delSink(const LogSink* sink)
{
	vector<LogSink*>::iterator it = find(m_sinks.begin(), m_sinks.end(), sink);

	if (it == m_sinks.end()) return;

	m_sinks.erase(it);
	delete sink;
}
