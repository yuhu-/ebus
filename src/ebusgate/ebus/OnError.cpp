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

#include "OnError.h"
#include "Connect.h"
#include "Listen.h"
#include "Logger.h"

OnError OnError::m_onError;

int OnError::run(EbusHandler* h)
{
	Logger& L = Logger::getLogger("OnError::run");

	if (h->m_lastResult > DEV_OK)
	{
		L.log(warn, "%s", h->m_device->errorText(h->m_lastResult).c_str());
	}
	else
	{
		L.log(error, "%s", h->m_device->errorText(h->m_lastResult).c_str());
	}

	if (m_activeMessage != nullptr) m_activeMessage->setResult(h->m_device->errorText(h->m_lastResult));

	reset(h);

	if (h->m_lastResult < 0)
	{
		h->m_device->close();

		if (h->m_device->isOpen() == false) L.log(info, "ebus disconnected");

		h->changeState(Connect::getInstance());
	}
	else
		h->changeState(Listen::getInstance());

	return (DEV_OK);
}

OnError::OnError()
{
}

const char* OnError::toString() const
{
	return ("OnError");
}
