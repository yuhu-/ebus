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

#ifndef PROCESS_PROCESS_H
#define PROCESS_PROCESS_H

#include "Notify.h"
#include <thread>
#include "../../ebuscpp/process/forward/Forward.h"
#include "../../ebuscpp/process/IProcess.h"

using std::thread;

class Process : public IProcess, public Notify
{

public:
	explicit Process(const unsigned char address);
	virtual ~Process();

	void start();
	void stop();

	virtual ActionType active(EbusSequence& eSeq) = 0;

	virtual void passive(EbusSequence& eSeq) = 0;

	void forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result);

protected:
	bool m_running = true;

	const unsigned char m_address;

private:
	thread m_thread;

	virtual void run() = 0;

};

#endif // PROCESS_PROCESS_H
