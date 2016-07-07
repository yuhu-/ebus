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

#include "Gateway.h"
#include "Logger.h"

Gateway::Gateway(const unsigned char address, const bool forward)
	: Process(address, forward)
{
}

ActionType Gateway::active(EbusSequence& eSeq)
{
	Logger logger = Logger("Gateway::active");
	logger.info("search %s", eSeq.toStringLog().c_str());

	if (eSeq.getMaster().contains("0700") == true)
	{
		return (at_ignore);
	}

	if (eSeq.getMaster().contains("0704") == true)
	{
		eSeq.createSlave("0a7a454741544501010101");
		return (at_response);
	}

	if (eSeq.getMaster().contains("07fe") == true)
	{
		eSeq.clear();
		eSeq.createMaster(m_address, BROADCAST, "07ff00");
		return (at_send);
	}

	if (eSeq.getMaster().contains("b505") == true)
	{
		return (at_ignore);
	}

	if (eSeq.getMaster().contains("b516") == true)
	{
		return (at_ignore);
	}

	return (at_undefined);
}

void Gateway::passive(EbusSequence& eSeq)
{
	Logger logger = Logger("Gateway::passive");
	logger.info("forward %s", eSeq.toStringLog().c_str());

	m_forward->enqueue(eSeq);

	// TODO handle passive message

}

void Gateway::run()
{
	Logger logger = Logger("Gateway::run");
	logger.info("started");

	while (m_running == true)
	{
		waitNotify();
		// TODO implement business logic
	}

	logger.info("stopped");
}

