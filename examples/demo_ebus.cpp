/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include <unistd.h>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <Ebus.h>
#include <ILogger.h>

class logger : public ebus::ILogger
{

public:
	void error(const std::string &message);
	void warn(const std::string &message);
	void info(const std::string &message);
	void debug(const std::string &message);
	void trace(const std::string &message);

};

void logger::error(const std::string &message)
{
	std::cout << "ERROR:   " << message << std::endl;
}

void logger::warn(const std::string &message)
{
	std::cout << "WARN:    " << message << std::endl;
}

void logger::info(const std::string &message)
{
	std::cout << "INFO:    " << message << std::endl;
}

void logger::debug(const std::string &message)
{
	//std::cout << "DEBUG:   " << message << std::endl;
}

void logger::trace(const std::string &message)
{
	//std::cout << "TRACE:   " << message << std::endl;
}

ebus::Reaction process(const std::string &message)
{
	std::cout << "process: " << message << std::endl;

	return (ebus::Reaction::undefined);
}

void publish(const std::string &message)
{
	std::cout << "publish: " << message << std::endl;
}

int main()
{

	ebus::Ebus service(std::byte(0xff), "/dev/ttyUSB0", std::make_shared<logger>(), std::bind(&process, std::placeholders::_1),
		std::bind(&publish, std::placeholders::_1));

	int count = 0;

	while (count < 10)
	{
		sleep(1);
		std::cout << "main: " << count << " seconds passed" << std::endl;

		count++;
	}

	return (0);

}