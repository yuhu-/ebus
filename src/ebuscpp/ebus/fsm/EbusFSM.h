/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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
#ifndef EBUS_FSM_EBUSFSM_H
#define EBUS_FSM_EBUSFSM_H

#include "EbusMessage.h"
#include "IProcess.h"
#include "EbusDevice.h"
#include "NQueue.h"
#include "Notify.h"

#include <fstream>
#include <thread>
#include <map>

using libutils::NQueue;
using std::ofstream;
using std::thread;
using std::map;

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
	friend class Evaluate;
	friend class SendMessage;
	friend class RecvResponse;
	friend class RecvMessage;
	friend class SendResponse;

public:
	EbusFSM(const unsigned char address, const string device, const bool noDeviceCheck, const long reopenTime,
		const long arbitrationTime, const long receiveTimeout, const int lockCounter, const int lockRetries,
		const bool raw, const bool dump, const string dumpFile, const long dumpFileMaxSize, IProcess* process);

	~EbusFSM();

	void start();
	void stop();

	void open();
	void close();

	bool getDump() const;
	void setDump(bool dump);

	bool getRaw() const;
	void setRaw(bool raw);

	void enqueue(EbusMessage* message);

private:
	thread m_thread;

	bool m_running = true;

	State* m_state = nullptr;
	State* m_forceState = nullptr;

	const unsigned char m_address;
	long m_reopenTime;

	long m_arbitrationTime;
	long m_receiveTimeout;

	int m_lockCounter;
	int m_lockRetries;

	int m_lastResult;

	EbusDevice* m_ebusDevice;

	bool m_raw = false;

	bool m_dump = false;
	string m_dumpFile;
	long m_dumpFileMaxSize;
	long m_dumpFileSize = 0;
	ofstream m_dumpRawStream;

	IProcess* m_process = nullptr;

	NQueue<EbusMessage*> m_ebusMsgQueue;

	void run();

	void changeState(State* state);

};

#endif // EBUS_FSM_EBUSFSM_H
