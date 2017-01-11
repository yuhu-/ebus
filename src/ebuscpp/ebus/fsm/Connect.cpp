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

#include "Connect.h"
#include "Idle.h"
#include "Listen.h"
#include "Logger.h"

#include <sstream>

using std::ostringstream;

Connect Connect::m_connect;

int Connect::run(EbusFSM* fsm)
{
	int result = DEV_OK;

	if (fsm->m_ebusDevice->isOpen() == false)
	{
		result = fsm->m_ebusDevice->open();

		if (fsm->m_ebusDevice->isOpen() == true && result == DEV_OK)
		{
			LIBLOGGER_INFO("%s", stateMessage(STATE_INF_EBUS_ON).c_str());
		}
		else
		{
			m_reopenTime++;
			if (m_reopenTime > fsm->m_reopenTime) fsm->changeState(Idle::getIdle());

			return (result);
		}
	}

	reset(fsm);

	fsm->changeState(Listen::getListen());
	return (result);
}

const string Connect::toString() const
{
	return ("Connect");
}

