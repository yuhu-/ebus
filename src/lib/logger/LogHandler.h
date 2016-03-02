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

#ifndef LIBLOGGER_LOGBASE_H
#define LIBLOGGER_LOGBASE_H

#include "LogMessage.h"
#include "LogSink.h"
#include "NQueue.h"

#include <string>
#include <vector>
#include <map>
#include <thread>

using std::string;
using std::vector;
using std::thread;

class LogHandler
{

public:
	static LogHandler& getLogHandler();
	~LogHandler();

	void start();
	void stop();

	Level getLevel() const;
	void setLevel(const Level& level);
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

	Level m_level = l_info;

	void run();

	void addSink(LogSink* sink);
	void delSink(const LogSink* sink);
};

#endif // LIBLOGGER_LOGBASE_H
