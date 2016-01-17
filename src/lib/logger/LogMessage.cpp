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

#include "LogMessage.h"

#include <algorithm>

#include <string.h>
#include <sys/time.h>

map<Level, string> LevelNames =
	{
		{ off, "OFF" },
		{ fatal, "FATAL" },
		{ error, "ERROR" },
		{ warn, "WARN" },
		{ info, "INFO" },
		{ debug, "DEBUG" },
		{ trace, "TRACE" },
		{ all, "ALL" } };

Level calcLevel(const string& level)
{
	for (const auto& lvl : LevelNames)
		if (strcasecmp(lvl.second.c_str(), level.c_str()) == 0) return (lvl.first);

	return (all);
}

Level LogMessage::getLevel() const
{
	return (m_level);
}

string LogMessage::getText() const
{
	return (m_text);
}

string LogMessage::getTime() const
{
	return (m_time);
}

LogMessage::LogMessage(const Level level, const string text)
	: m_level(level), m_text(text)
{
	char time[24];
	struct timeval tv;
	struct timezone tz;
	struct tm* tm;

	gettimeofday(&tv, &tz);
	tm = localtime(&tv.tv_sec);

	sprintf(&time[0], "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
		tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);

	m_time = string(time);
}
