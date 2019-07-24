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

#include <chrono>
#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "Average.h"
#include "Device.h"
#include "Message.h"
#include "NQueue.h"
#include "Sequence.h"
#include "Telegram.h"

namespace ebus
{

class ILogger;

enum class Reaction
{
	nofunction,	// no function
	undefined,	// message undefined
	ignore,		// message ignored
	response	// send response
};

enum class State
{
	IdleSystem,
	OpenDevice,
	MonitorBus,
	ReceiveMessage,
	ProcessMessage,
	SendResponse,
	LockBus,
	SendMessage,
	ReceiveResponse,
	FreeBus
};

class Ebus : private Notify
{

public:
	Ebus(const std::byte address, const std::string &device, std::shared_ptr<ILogger> logger,
		std::function<Reaction(const std::string&)> process, std::function<void(const std::string&)> publish);

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

private:
	std::thread m_thread;

	bool m_running = true;
	bool m_online = false;
	bool m_close = false;

	const std::byte m_address;                       // ebus master address
	const std::byte m_slaveAddress;                  // ebus slave address

	long m_reopenTime = 10L;                         // max. time to open ebus device [s]
	long m_arbitrationTime = 5000L;                  // waiting time for arbitration test [us]
	long m_receiveTimeout = 10000L;                  // max. time for receiving of one sequence sign [us]
	int m_lockCounter = 5;                           // number of characters after a successful ebus access (max: 25)
	int m_lockRetries = 2;                           // number of retries to lock ebus

	bool m_dump = false;                             // enable/disable raw data dumping
	std::string m_dumpFile = "/tmp/ebus_dump.bin";   // dump file name
	long m_dumpFileMaxSize = 100L;                   // max size for dump file [kB]
	long m_dumpFileSize = 0L;                        // current size of dump file
	std::ofstream m_dumpRawStream;                   // dump stream

	long m_lastSeconds =
		std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	long m_bytes = 0;
	long m_bytesPerSeconds = 0;
	std::unique_ptr<Average> m_bytesPerSecondsAVG = nullptr;

	NQueue<std::shared_ptr<Message>> m_messageQueue;

	std::unique_ptr<Device> m_device = nullptr;
	std::shared_ptr<ILogger> m_logger = nullptr;

	std::function<Reaction(const std::string&)> m_process;
	std::function<void(const std::string&)> m_publish;

	long m_curReopenTime = 0;
	int m_curLockCounter = 0;
	int m_curLockRetries = 0;
	Sequence m_sequence;
	std::shared_ptr<Message> m_activeMessage = nullptr;
	std::shared_ptr<Message> m_passiveMessage = nullptr;

	int transmit(Telegram &tel);

	void read(std::byte &byte, const long sec, const long nsec);
	void write(const std::byte &byte);
	void writeRead(const std::byte &byte, const long sec, const long nsec);

	void reset();

	const std::string stateMessage(const int state);
	const std::string telegramInfo(Telegram &tel);

	void run();

	State idleSystem();
	State openDevice();
	State monitorBus();
	State receiveMessage();
	State processMessage();
	State sendResponse();
	State lockBus();
	State sendMessage();
	State receiveResponse();
	State freeBus();

	State handleDeviceError(bool error, const std::string &message);

	Reaction process(const std::string &message);
	void publish(const std::string &message);

	void dumpByte(const std::byte &byte);
	void countByte();

	void logError(const std::string &message);
	void logWarn(const std::string &message);
	void logInfo(const std::string &message);
	void logDebug(const std::string &message);
	void logTrace(const std::string &message);

};

} // namespace ebus

#endif // EBUS_EBUS_H
