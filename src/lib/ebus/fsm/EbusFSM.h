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
#ifndef LIBEBUS_FSM_EBUSFSM_H
#define LIBEBUS_FSM_EBUSFSM_H

#include "IEbusProcess.h"
#include "IEbusLogger.h"
#include "EbusDevice.h"

#include <fstream>
#include <thread>
#include <map>

using libutils::NQueue;
using std::ofstream;
using std::thread;
using std::map;

namespace libebus
{

class State;

class EbusFSM : public Notify
{
	friend class State;
	friend class OnError;
	friend class Idle;
	friend class Connect;
	friend class Listen;
	friend class LockBus;
	friend class FreeBus;
	friend class EvalMessage;
	friend class SendMessage;
	friend class RecvResponse;
	friend class RecvMessage;
	friend class SendResponse;

public:
	EbusFSM(const unsigned char address, const string device, const bool deviceCheck, IEbusProcess* process,
		IEbusLogger* logger);

	~EbusFSM();

	void start();
	void stop();

	void open();
	void close();

	long getReopenTime() const;
	void setReopenTime(const long& reopenTime);

	long getArbitrationTime() const;
	void setArbitrationTime(const long& arbitrationTime);

	long getReceiveTimeout() const;
	void setReceiveTimeout(const long& receiveTimeout);

	int getLockCounter() const;
	void setLockCounter(const int& lockCounter);

	int getLockRetries() const;
	void setLockRetries(const int& lockRetries);

	bool getDump() const;
	void setDump(const bool& dump);

	string getDumpFile() const;
	void setDumpFile(const string& dumpFile);

	long getDumpFileMaxSize() const;
	void setDumpFileMaxSize(const long& dumpFileMaxSize);

private:
	thread m_thread;

	bool m_running = true;

	State* m_state = nullptr;
	State* m_forceState = nullptr;

	int m_lastResult = DEV_OK;

	const unsigned char m_address;              // ebus master address
	const unsigned char m_slaveAddress;         // ebus slave address

	long m_reopenTime = 60;                     // max. time to open ebus device [s]
	long m_arbitrationTime = 4400;              // waiting time for arbitration test [us]
	long m_receiveTimeout = 4700;               // max. time for receiving of one sequence sign [us]
	int m_lockCounter = 5;                      // number of characters after a successful ebus access (max: 25)
	int m_lockRetries = 2;                      // number of retries to lock ebus

	bool m_dump = false;                        // enable/disable raw data dumping
	string m_dumpFile = "/tmp/ebus_dump.bin";   // dump file name
	long m_dumpFileMaxSize = 100;               // max size for dump file [kB]
	long m_dumpFileSize = 0;                    // current size of dump file
	ofstream m_dumpRawStream;

	EbusDevice* m_ebusDevice = nullptr;
	IEbusProcess* m_process = nullptr;
	IEbusLogger* m_logger = nullptr;

	void run();

	void changeState(State* state);

	Action getEvaluatedAction(EbusSequence& eSeq);
	void evalActiveMessage(EbusSequence& eSeq);
	void evalPassiveMessage(EbusSequence& eSeq);
	EbusMessage* dequeueProcessMessage();
	size_t getProcessQueueSize();

	void logError(const string& message);
	void logWarn(const string& message);
	void logInfo(const string& message);
	void logDebug(const string& message);
	void logTrace(const string& message);

};

} // namespace libebus

#endif // LIBEBUS_FSM_EBUSFSM_H
