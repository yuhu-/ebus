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

#ifndef LOGGER_LOGMESSAGE_H
#define LOGGER_LOGMESSAGE_H

#include <string>
#include <map>

using std::string;
using std::map;

enum Level
{
	off = 0x01,
	fatal = 0x02,
	error = 0x04,
	warn = 0x08,
	info = 0x10,
	debug = 0x20,
	trace = 0x40,
	all = 0x80
};

Level calcLevel(const string& level);

class LogMessage
{

public:
	LogMessage(const Level level, const string text);

	Level getLevel() const;
	string getText() const;
	string getTime() const;

private:
	Level m_level;

	string m_text;
	string m_time;

};

#endif // LOGGER_LOGMESSAGE_H
