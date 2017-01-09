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

#include "Logger.h"

#include <cstdarg>

using namespace liblogger;

Logger::Logger(const string& function)
	: m_logHandler(LogHandler::getLogHandler()), m_function(function)
{
}

void Logger::start()
{
	m_logHandler.start();
}

void Logger::stop()
{
	m_logHandler.stop();
}

void Logger::setLevel(const string& level)
{
	m_logHandler.setLevel(level);
}

void Logger::addConsole()
{
	m_logHandler.addConsole();
}

void Logger::addFile(const string& file)
{
	m_logHandler.addFile(file);
}

void Logger::log(const Level level, const string data, ...)
{
	if (m_logHandler.getLevel() >= level)
	{
		char* tmp;
		va_list ap;
		va_start(ap, data);

		if (vasprintf(&tmp, data.c_str(), ap) != -1)
		{
			string buffer(tmp);
			m_logHandler.log(new LogMessage(m_function, m_logHandler.getLevelName(level), buffer));
		}

		va_end(ap);
		free(tmp);
	}
}
