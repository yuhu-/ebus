/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#ifndef LIBLOGGER_LOGHANDLER_H
#define LIBLOGGER_LOGHANDLER_H

#include "LogMessage.h"
#include "LogSink.h"
#include "NQueue.h"

#include <map>
#include <string>
#include <vector>
#include <thread>

using libutils::NQueue;
using std::map;
using std::string;
using std::vector;
using std::thread;

enum class Level
{
	off = 0x00, error = 0x01, warn = 0x02, info = 0x04, debug = 0x08, trace = 0x10
};

class LogHandler
{

	map<Level, string> LevelNames =
	{
	{ Level::off, "OFF" },
	{ Level::error, "ERROR" },
	{ Level::warn, "WARN" },
	{ Level::info, "INFO" },
	{ Level::debug, "DEBUG" },
	{ Level::trace, "TRACE" } };

public:
	static LogHandler& getLogHandler();
	~LogHandler();

	void start();
	void stop();

	Level getLevel() const;
	string getLevelName(Level level);
	void setLevel(const string& level);

	void addConsole();
	void addFile(const string& file);

	void log(const LogMessage* logMessage);

private:
	LogHandler();
	LogHandler(const LogHandler&);
	LogHandler& operator=(const LogHandler&);

	thread m_thread;

	vector<LogSink*> m_sinks;

	NQueue<const LogMessage*> m_logMessages;

	Level m_level = Level::info;

	void run();

	Level findLevel(const string& level);

	void addSink(LogSink* sink);
	void delSink(const LogSink* sink);
};

#endif // LIBLOGGER_LOGHANDLER_H
