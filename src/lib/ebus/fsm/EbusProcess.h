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

using std::thread;

namespace libebus
{

class EbusProcess : public IEbusProcess, public Notify
{

public:
	explicit EbusProcess(const unsigned char address);
	virtual ~EbusProcess();

	void start();
	void stop();

	virtual const string sendMessage(const string& message) final;

protected:
	bool m_running = true;

	const unsigned char m_address;
	const unsigned char m_slaveAddress;

	virtual Action identifyAction(EbusSequence& eSeq) = 0;
	virtual void handleActiveMessage(EbusSequence& eSeq) = 0;
	virtual void handlePassiveMessage(EbusSequence& eSeq) = 0;

	virtual void createMessage(EbusSequence& eSeq) final;
	virtual EbusMessage* processMessage() final;

private:
	thread m_thread;

	virtual void run() = 0;

};

} // namespace libebus

#endif // LIBEBUS_FSM_EBUSPROCESS_H
