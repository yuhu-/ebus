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

#ifndef LIBLOGGER_LOGMESSAGE_H
#define LIBLOGGER_LOGMESSAGE_H

#include <string>
#include <map>

using std::string;
using std::map;

enum Level
{
	off = 0x00, error = 0x01, warn = 0x02, info = 0x04, debug = 0x08, trace = 0x10
};

Level calcLevel(const string& level);

class LogMessage
{

public:
	LogMessage(const string function, const Level level, const string text);

	string getFunction() const;
	Level getLevel() const;
	string getText() const;
	string getTime() const;

private:
	string m_function;
	Level m_level;

	string m_text;
	string m_time;

};

#endif // LIBLOGGER_LOGMESSAGE_H
