/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#include "OnError.h"
#include "Connect.h"
#include "Listen.h"
#include "Logger.h"

OnError OnError::m_onError;

int OnError::run(EbusFSM* fsm)
{
	Logger logger = Logger("OnError::run");

	if (fsm->m_lastResult > DEV_OK)
		logger.warn("%s", fsm->m_ebusDevice->errorText(fsm->m_lastResult).c_str());
	else
		logger.error("%s", fsm->m_ebusDevice->errorText(fsm->m_lastResult).c_str());

	if (m_activeMessage != nullptr) m_activeMessage->setResult(fsm->m_ebusDevice->errorText(fsm->m_lastResult));

	reset(fsm);

	if (fsm->m_lastResult < 0)
	{
		fsm->m_ebusDevice->close();

		if (fsm->m_ebusDevice->isOpen() == false) logger.info("ebus disconnected");

		fsm->changeState(Connect::getConnect());
	}
	else
		fsm->changeState(Listen::getListen());

	return (DEV_OK);
}

const string OnError::toString() const
{
	return ("OnError");
}
