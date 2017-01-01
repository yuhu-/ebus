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

#include "LogMessage.h"

#include <algorithm>
#include <cstring>
//#include <sstream>
//#include <chrono>
//#include <iomanip>

#include <sys/time.h>

//using std::ostringstream;
//using std::setw;
//using std::setfill;

string LogMessage::getFunction() const
{
	return (m_function);
}

string LogMessage::getLevel() const
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

LogMessage::LogMessage(const string& function, const string& level, const string& text)
	: m_function(function), m_level(level), m_text(text)
{
	char time[24];
	struct timeval tv;
	struct timezone tz;
	struct tm* tm;

	gettimeofday(&tv, &tz);
	tm = localtime(&tv.tv_sec);

	sprintf(&time[0], "%04d-%02d-%02d %02d:%02d:%02d.%03ld", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);

	m_time = string(time);
}

//LogMessage::LogMessage(const Level level, const string text)
//	: m_level(level), m_text(text)
//{
//	auto now = std::chrono::system_clock::now();
//	auto in_time_t = std::chrono::system_clock::to_time_t(now);
//	auto ms = std::chrono::duration_cast < std::chrono::milliseconds
//		> (now.time_since_epoch());
//
//	ostringstream ostr;
//	ostr << std::put_time(localtime(&in_time_t), "%Y-%m-%d %X.") << setw(3)
//		<< setfill('0') << ms.count() % 1000;
//	m_time =  ostr.str();
//}

