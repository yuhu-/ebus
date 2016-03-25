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
#include "EbusDevice.h"
#include "DataHandler.h"
#include "NQueue.h"
#include "Notify.h"

#include <fstream>
#include <thread>
#include <map>

using std::ofstream;
using std::thread;
using std::map;

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
	friend class Reaction;
	friend class SendMessage;
	friend class RecvResponse;
	friend class RecvMessage;
	friend class SendResponse;

public:
	EbusHandler(const unsigned char address, const string device, const bool noDeviceCheck, const long reopenTime,
		const long arbitrationTime, const long receiveTimeout, const int lockCounter, const int lockRetries,
		const bool active, const bool dumpRaw, const string dumpRawFile, const long dumpRawFileMaxSize,
		const bool logRaw);

	~EbusHandler();

	void start();
	void stop();

	void open();
	void close();

	bool getActive();
	void setActive(bool active);

	bool getDumpRaw() const;
	void setDumpRaw(bool dumpRaw);

	bool getLogRaw();
	void setLogRaw(bool logRaw);

	void enqueue(EbusMessage* message);

	bool subscribe(const string& ip, long port, const string& filter, ostringstream& result);
	bool unsubscribe(const string& ip, long port, const string& filter, ostringstream& result);

private:
	thread m_thread;

	bool m_running = true;

	DataHandler* m_dataHandler = nullptr;

	State* m_state = nullptr;
	State* m_forceState = nullptr;

	const unsigned char m_address;
	long m_reopenTime;

	long m_arbitrationTime;
	long m_receiveTimeout;

	int m_lockCounter;
	int m_lockRetries;

	bool m_active;
	bool m_activeDone = false;

	int m_lastResult;

	EbusDevice* m_device;

	bool m_dumpRaw = false;
	string m_dumpRawFile;
	long m_dumpRawFileMaxSize;
	long m_dumpRawFileSize = 0;
	ofstream m_dumpRawStream;

	bool m_logRaw = false;

	NQueue<EbusMessage*> m_ebusMsgQueue;

	void run();

	void changeState(State* state);

};

#endif // EBUS_EBUSHANDLER_H
