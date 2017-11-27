/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebusfsm.
 *
 * ebusfsm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusfsm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusfsm. If not, see http://www.gnu.org/licenses/.
 */

#ifndef EBUSFSM_IEBUSLOGGER_H
#define EBUSFSM_IEBUSLOGGER_H

#include <string>

namespace ebusfsm
{

class IEbusLogger
{

public:
	virtual ~IEbusLogger()
	{
	}

	virtual void error(const std::string& message) = 0;
	virtual void warn(const std::string& message) = 0;
	virtual void info(const std::string& message) = 0;
	virtual void debug(const std::string& message) = 0;
	virtual void trace(const std::string& message) = 0;

};

} // namespace ebusfsm

#endif // EBUSFSM_IEBUSLOGGER_H
