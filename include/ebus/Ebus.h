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

#ifndef EBUS_EBUS_H
#define EBUS_EBUS_H

#include <cstddef>
#include <experimental/propagate_const>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace ebus
{

class ILogger
{

public:
	virtual ~ILogger()
	{
	}

	virtual void error(const std::string &message) = 0;
	virtual void warn(const std::string &message) = 0;
	virtual void info(const std::string &message) = 0;
	virtual void debug(const std::string &message) = 0;
	virtual void trace(const std::string &message) = 0;

};

enum class Reaction
{
	nofunction,	// no function
	undefined,	// message undefined
	ignore,		// message ignored
	response	// send response
};

class Ebus
{

public:
	Ebus(const std::byte address, const std::string &device, std::shared_ptr<ILogger> logger,
		std::function<Reaction(const std::string &message, std::string &response)> process,
		std::function<void(const std::string &message)> publish);

	// Move functions declared
	Ebus& operator=(Ebus&&);
	Ebus(Ebus&&);

	// Copy functions declared and defined here
	Ebus& operator=(const Ebus&) = delete;
	Ebus(const Ebus&) = delete;

	~Ebus();

	void open();
	void close();

	bool isOnline();

	int transmit(const std::string &message, std::string &response);
	int transmit(const std::string &message, std::vector<std::byte> &response);

	const std::string errorText(const int error) const;

	void setReopenTime(const long &reopenTime);
	void setArbitrationTime(const long &arbitrationTime);
	void setReceiveTimeout(const long &receiveTimeout);
	void setLockCounter(const int &lockCounter);
	void setLockRetries(const int &lockRetries);

	void setDump(const bool &dump);
	void setDumpFile(const std::string &dumpFile);
	void setDumpFileMaxSize(const long &dumpFileMaxSize);

	long actBusSpeed() const;
	double avgBusSpeed() const;

	static const std::vector<std::byte> toVector(const std::string &str);
	static const std::string toString(const std::vector<std::byte> &seq);
	static bool isHex(const std::string &str, std::ostringstream &result, const int &nibbles);

private:
	class EbusImpl;
	std::experimental::propagate_const<std::unique_ptr<EbusImpl>> impl;

};

} // namespace ebus

#endif // EBUS_EBUS_H
