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

#ifndef LIBEBUS_FSM_ILOGGER_H
#define LIBEBUS_FSM_ILOGGER_H

#include <string>

using std::string;

namespace libebus
{

class ILogger
{

public:
	virtual ~ILogger()
	{
	}

	virtual void error(const string& message) = 0;
	virtual void warn(const string& message) = 0;
	virtual void info(const string& message) = 0;
	virtual void debug(const string& message) = 0;
	virtual void trace(const string& message) = 0;

};

} // namespace libebus

#endif // LIBEBUS_FSM_ILOGGER_H
