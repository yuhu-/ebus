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

#include "Listen.h"
#include "LockBus.h"
#include "RecvMessage.h"
#include "Common.h"
#include "Logger.h"

extern Logger& L;

Listen Listen::m_listen;

int Listen::run(EbusHandler* h)
{
	unsigned char byte = 0;

	int result = read(h, byte, 1, 0);
	if (result != DEV_OK) return (result);

	if (byte == SYN)
	{
		if (m_lockCounter != 0)
		{
			m_lockCounter--;
			L.log(trace, "lockCounter: %d", m_lockCounter);
		}

		// decode EbusSequence
		if (m_sequence.size() != 0)
		{
			L.log(debug, "%s", m_sequence.toString().c_str());

			EbusSequence eSeq(m_sequence);
			L.log(info, "%s", eSeq.toStringLog().c_str());

			if (eSeq.isValid() == true)
			{
				if (h->m_store == true)
					h->m_ebusDataStore->write(eSeq);
			}

			if (m_sequence.size() == 1 && m_lockCounter < 2)
				m_lockCounter = 2;

			eSeq.clear();
			m_sequence.clear();
		}

		// check for new EbusMessage
		if (m_activeMessage == nullptr && h->m_ebusMsgQueue.size() != 0)
		{
			L.log(trace, "pending ebus messages: %d",
				h->m_ebusMsgQueue.size());
			m_activeMessage = h->m_ebusMsgQueue.dequeue();
		}

		// handle EbusMessage
		if (m_activeMessage != nullptr && m_lockCounter == 0)
			h->changeState(LockBus::getInstance());
	}
	else
	{
		m_sequence.push_back(byte);

		// handle broadcast and at me addressed messages
		if (m_sequence.size() == 2)
		{
			if (m_sequence[1] == BROADCAST
				|| m_sequence[1] == h->m_address
				|| m_sequence[1] == slaveAddress(h->m_address))
				h->changeState(RecvMessage::getInstance());
		}

	}

	return (result);
}

Listen::Listen()
{
}

const char* Listen::toString() const
{
	return ("Listen");
}

