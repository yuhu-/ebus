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

#ifndef PROCESS_PROCESS_H
#define PROCESS_PROCESS_H

#include "Forward.h"
#include "IProcess.h"
#include "Notify.h"

#include <thread>

using libebus::IProcess;
using libebus::Action;
using libebus::EbusSequence;
using libebus::EbusMessage;
using std::thread;

class Process : public IProcess, public Notify
{

public:
	explicit Process(const unsigned char address);
	virtual ~Process();

	void start();
	void stop();

protected:
	bool m_running = true;

	const unsigned char m_address;
	const unsigned char m_slaveAddress;

	virtual Action activeMessage(EbusSequence& eSeq) = 0;
	virtual void passiveMessage(EbusSequence& eSeq) = 0;

	void createMessage(EbusSequence& eSeq);
	EbusMessage* processMessage();
	size_t pendingMessages();

private:
	thread m_thread;

	NQueue<EbusMessage*> m_ebusMsgProcessQueue;

	virtual void run() = 0;

};

#endif // PROCESS_PROCESS_H
