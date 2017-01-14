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
using std::thread;

class Process : public IProcess, public Notify
{

public:
	explicit Process(const unsigned char address);
	virtual ~Process();

	void start();
	void stop();

	virtual Action handleActiveMessage(EbusSequence& eSeq) = 0;

	virtual void handlePassiveMessage(EbusSequence& eSeq) = 0;

	virtual void handleProcessMessage(EbusSequence& eSeq) = 0;

protected:
	bool m_running = true;

	const unsigned char m_address;

private:
	thread m_thread;

	virtual void run() = 0;

};

#endif // PROCESS_PROCESS_H
