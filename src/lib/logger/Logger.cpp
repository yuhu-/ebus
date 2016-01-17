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

#include "Logger.h"
#include "LogConsole.h"
#include "LogFile.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdarg>
#include <sstream>

using std::istringstream;
using std::stringstream;
using std::endl;
using std::setw;
using std::setfill;
using std::left;

extern map<Level, string> LevelNames;

Logger& Logger::getInstance()
{
	static Logger instance;
	return (instance);
}

Logger::~Logger()
{
	while (m_sinks.size() > 0)
		delSink(*(m_sinks.begin()));
}

void Logger::start()
{
	m_thread = thread(&Logger::run, this);
}

void Logger::stop()
{
	if (m_thread.joinable())
	{
		m_logMessages.enqueue(NULL);
		m_thread.join();
	}
}

void Logger::setLevel(const Level& level)
{
	m_level = level;
}
void Logger::setLevel(const string& level)
{
	m_level = calcLevel(level);
}

void Logger::addConsole()
{
	addSink(new LogConsole());
}

void Logger::addFile(const char* file)
{
	addSink(new LogFile(file));
}

void Logger::log(const Level level, const string& data, ...)
{
	if (m_level != off)
	{
		char* tmp;
		va_list ap;
		va_start(ap, data);

		if (vasprintf(&tmp, data.c_str(), ap) != -1)
		{
			string buffer(tmp);
			m_logMessages.enqueue(new LogMessage(level, buffer));
		}

		va_end(ap);
		free(tmp);
	}
}


void Logger::run()
{
	LogMessage* message;

	while (true)
	{
		message = m_logMessages.dequeue();
		if (message == NULL) break;

		if (m_level >= message->getLevel())
		{
			stringstream sstr;

			sstr << "[" << message->getTime() << "] " << setw(5)
				<< setfill(' ') << left
				<< LevelNames[(Level) message->getLevel()]
				<< " " << message->getText() << endl;

			for (const auto& sink : m_sinks)
				sink->write(sstr.str());
		}

		delete message;
	}
}

void Logger::addSink(LogSink* sink)
{
	vector<LogSink*>::iterator it = find(m_sinks.begin(), m_sinks.end(),
		sink);

	if (it == m_sinks.end()) m_sinks.push_back(sink);

}

void Logger::delSink(const LogSink* sink)
{
	vector<LogSink*>::iterator it = find(m_sinks.begin(), m_sinks.end(),
		sink);

	if (it == m_sinks.end()) return;

	m_sinks.erase(it);
	delete sink;
}
