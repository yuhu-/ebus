/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
 *
 * This file is part of ebusfsm.
 *
 * ebusfsm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusfsm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusfsm. If not, see http://www.gnu.org/licenses/.
 */

#include <FreeBus.h>
#include <Listen.h>

ebusfsm::FreeBus ebusfsm::FreeBus::m_freeBus;

int ebusfsm::FreeBus::run(EbusFSM* fsm)
{
	unsigned char byte = SYN;

	int result = writeRead(fsm, byte, 0, 0);
	if (result != DEV_OK) return (result);

	fsm->logDebug(stateMessage(fsm, STATE_INF_EBUS_FREE));

	reset(fsm);
	fsm->changeState(Listen::getListen());

	return (result);
}

const std::string ebusfsm::FreeBus::toString() const
{
	return ("FreeBus");
}
