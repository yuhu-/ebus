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
#include <sstream>
#include <chrono>
#include <iomanip>

#include <sys/time.h>

using std::ostringstream;
using std::setw;
using std::setfill;

string liblogger::LogMessage::getFunction() const
{
	return (m_function);
}

string liblogger::LogMessage::getLevel() const
{
	return (m_level);
}

string liblogger::LogMessage::getText() const
{
	return (m_text);
}

string liblogger::LogMessage::getTime() const
{
	return (m_time);
}

liblogger::LogMessage::LogMessage(const string& function, const string& level, const string& text)
	: m_function(function), m_level(level), m_text(text)
{
	auto now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

	ostringstream ostr;
	ostr << std::put_time(localtime(&in_time_t), "%Y-%m-%d %X.") << setw(3) << setfill('0') << ms.count() % 1000;
	m_time = ostr.str();
}

