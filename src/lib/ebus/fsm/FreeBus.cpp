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

#include "FreeBus.h"
#include "Listen.h"

libebus::FreeBus libebus::FreeBus::m_freeBus;

int libebus::FreeBus::run(EbusFSM* fsm)
{
	unsigned char byte = SYN;

	int result = writeRead(fsm, byte, 0);
	if (result != DEV_OK) return (result);

	fsm->m_logger->debug(stateMessage(STATE_INF_EBUS_FREE));

	reset(fsm);
	fsm->changeState(Listen::getListen());

	return (result);
}

const string libebus::FreeBus::toString() const
{
	return ("FreeBus");
}
