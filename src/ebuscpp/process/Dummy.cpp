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

#include "Dummy.h"
#include "Logger.h"

Dummy::Dummy(const unsigned char address)
	: Process(address)
{
}

Dummy::~Dummy()
{
}

Action Dummy::active(EbusSequence& eSeq)
{
	LIBLOGGER_INFO("handle active %s", eSeq.toStringLog().c_str());

	if (eSeq.getMaster().contains("0700") == true)
	{
		return (Action::ignore);
	}

	if (eSeq.getMaster().contains("07fe") == true)
	{
		eSeq.clear();
		eSeq.createMaster(m_address, BROADCAST, "07ff00");
		return (Action::send);
	}

	// TODO implement active message handler

	return (Action::undefined);
}

void Dummy::passive(EbusSequence& eSeq)
{
	LIBLOGGER_INFO("handle passive %s", eSeq.toStringLog().c_str());

	// TODO implement passive message handler
}

void Dummy::run()
{
	LIBLOGGER_INFO("started");

	while (m_running == true)
	{
		waitNotify();
		// TODO implement business logic
	}

	LIBLOGGER_INFO("stopped");
}

