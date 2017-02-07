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

#ifndef LIBEBUS_FSM_EBUSPROCESS_H
#define LIBEBUS_FSM_EBUSPROCESS_H

#include "IEbusProcess.h"
#include "Notify.h"

#include <thread>

using libebus::IEbusProcess;
using libebus::Action;
using libebus::EbusSequence;
using libebus::EbusMessage;
using std::thread;

class EbusProcess : public IEbusProcess, public Notify
{

public:
	explicit EbusProcess(const unsigned char address);
	virtual ~EbusProcess();

	void start();
	void stop();

	virtual void enqueueMessage(EbusMessage* message) final;

protected:
	bool m_running = true;

	const unsigned char m_address;
	const unsigned char m_slaveAddress;

	virtual Action getEvaluatedAction(EbusSequence& eSeq) = 0;
	virtual void evalActiveMessage(EbusSequence& eSeq) = 0;
	virtual void evalPassiveMessage(EbusSequence& eSeq) = 0;

	virtual void createMessage(EbusSequence& eSeq) final;
	virtual EbusMessage* processMessage() final;
	virtual size_t pendingMessages() final;

private:
	thread m_thread;

	NQueue<EbusMessage*> m_ebusMsgQueue;

	virtual void run() = 0;

};

#endif // LIBEBUS_FSM_EBUSPROCESS_H