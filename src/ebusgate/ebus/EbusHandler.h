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

#ifndef EBUS_EBUSHANDLER_H
#define EBUS_EBUSHANDLER_H

#include "EbusMessage.h"
#include "EbusDataStore.h"
#include "EbusDevice.h"
#include "NQueue.h"
#include "Notify.h"

#include <fstream>
#include <thread>

using std::ofstream;
using std::thread;

class State;

class EbusHandler : public Notify
{
	friend class State;
	friend class OnError;
	friend class Idle;
	friend class Connect;
	friend class Listen;
	friend class LockBus;
	friend class FreeBus;
	friend class SendMessage;
	friend class RecvResponse;
	friend class SendResponse;

public:
	EbusHandler(const unsigned char address, const string device,
		const bool noDeviceCheck, const long reopenTime,
		const long arbitrationTime, const long receiveTimeout,
		const int lockCounter, const int lockRetries,
		const bool response, const bool store, const bool logRaw,
		const bool dumpRaw, const char* dumpRawFile,
		const long dumpRawFileMaxSize);

	~EbusHandler();

	void start();
	void stop();

	void open();
	void close();

	bool getLogRaw();
	void setLogRaw(const bool logRaw);

	bool getDumpRaw() const;
	void setDumpRaw(const bool dumpRaw);

	void setDumpRawFile(const string& dumpFile);
	void setDumpRawMaxSize(const long maxSize);

	void addMessage(EbusMessage* message);
	const string lookMessage(const string& str) const;

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
	bool m_response;
	bool m_store;

	int m_lastResult;

	EbusDevice* m_device;

	bool m_logRaw = false;
	bool m_dumpRaw = false;

	string m_dumpRawFile;

	long m_dumpRawFileMaxSize;
	long m_dumpRawFileSize = 0;

	ofstream m_dumpRawStream;

	NQueue<EbusMessage*> m_ebusMsgQueue;

	EbusDataStore* m_ebusDataStore = nullptr;

	void run();

	void changeState(State* state);

};

#endif // EBUS_EBUSHANDLER_H
