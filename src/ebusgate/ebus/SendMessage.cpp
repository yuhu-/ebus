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

#include "SendMessage.h"
#include "Listen.h"
#include "FreeBus.h"
#include "RecvResponse.h"
#include "Logger.h"

SendMessage SendMessage::m_sendMessage;

int SendMessage::run(EbusHandler* h)
{
	Logger L = Logger("SendMessage::run");

	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	int result;

	for (int retry = 1; retry >= 0; retry--)
	{
		// send Message
		for (size_t i = retry; i < eSeq.getMaster().size(); i++)
		{
			result = writeRead(h, eSeq.getMaster()[i], 0);
			if (result != DEV_OK) return (result);
		}

		// send CRC
		result = writeRead(h, eSeq.getMasterCRC(), 0);
		if (result != DEV_OK) return (result);

		// Broadcast ends here
		if (eSeq.getType() == EBUS_TYPE_BC)
		{
			L.log(info, "%s done", eSeq.toStringLog().c_str());
			h->changeState(FreeBus::getFreeBus());
			break;
		}

		unsigned char byte;

		// receive ACK
		int result = read(h, byte, 0, h->m_receiveTimeout);
		if (result != DEV_OK) return (result);

		if (byte != ACK && byte != NAK)
		{
			L.log(warn, "%s", errorText(STATE_ERR_ACK_WRONG).c_str());
			m_activeMessage->setResult(errorText(STATE_ERR_ACK_WRONG));

			h->changeState(FreeBus::getFreeBus());
			break;
		}
		else if (byte == ACK)
		{
			// Master Master ends here
			if (eSeq.getType() == EBUS_TYPE_MM)
			{
				L.log(info, "%s done", eSeq.toStringLog().c_str());
				h->changeState(FreeBus::getFreeBus());
			}
			else
			{
				h->changeState(RecvResponse::getRecvResponse());
			}

			break;
		}
		else
		{
			if (retry == 1)
			{
				L.log(debug, "%s", errorText(STATE_WRN_ACK_NEG).c_str());
			}
			else
			{
				L.log(warn, "%s", errorText(STATE_ERR_ACK_NEG).c_str());
				m_activeMessage->setResult(errorText(STATE_ERR_ACK_NEG));

				h->changeState(FreeBus::getFreeBus());
			}
		}
	}

	return (result);
}

SendMessage::SendMessage()
{
}

const string SendMessage::toString() const
{
	return ("SendMessage");
}
