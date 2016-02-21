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

#include "RecvResponse.h"
#include "FreeBus.h"
#include "Listen.h"
#include "Logger.h"

RecvResponse RecvResponse::m_recvResponse;

int RecvResponse::run(EbusHandler* h)
{
	Logger& L = Logger::getLogger("RecvResponse::run");

	EbusSequence& eSeq = m_activeMessage->getEbusSequence();
	unsigned char byte;
	Sequence seq;
	int result;

	for (int retry = 1; retry >= 0; retry--)
	{
		// receive NN
		result = read(h, byte, 1, 0);
		if (result != DEV_OK) return (result);

		// check against max. possible size
		if (byte > 0x10)
		{
			L.log(warn, "%s", errorText(STATE_ERR_NN_WRONG).c_str());
			reset(h);
			h->changeState(Listen::getListen());
			return (DEV_OK);
		}

		seq.push_back(byte);

		// +1 for CRC
		size_t bytes = byte + 1;

		for (size_t i = 0; i < bytes; i++)
		{
			result = read(h, byte, 1, 0);
			if (result != DEV_OK) return (result);

			seq.push_back(byte);

			if (byte == SYN || byte == EXT) bytes++;
		}

		// create slave data
		eSeq.createSlave(seq);

		if (eSeq.getSlaveState() == EBUS_OK)
		{
			byte = ACK;
		}
		else
		{
			byte = NAK;
		}

		eSeq.setMasterACK(byte);

		// send ACK
		result = writeRead(h, byte, 0);
		if (result != DEV_OK) return (result);

		if (eSeq.getSlaveState() == EBUS_OK)
		{
			L.log(info, "%s done", eSeq.toStringLog().c_str());
			break;
		}

		if (retry == 1)
		{
			seq.clear();
			L.log(debug, "%s", errorText(STATE_WRN_RECV_RESP).c_str());
		}
		else
		{
			L.log(warn, "%s", errorText(STATE_ERR_RECV_RESP).c_str());
			m_activeMessage->setResult(errorText(STATE_ERR_RECV_RESP));
		}
	}

	h->changeState(FreeBus::getFreeBus());

	return (result);
}

RecvResponse::RecvResponse()
{
}

const string RecvResponse::toString() const
{
	return ("RecvResponse");
}
