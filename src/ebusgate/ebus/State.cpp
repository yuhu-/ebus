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

#include "State.h"
#include "EbusHandler.h"
#include "Color.h"
#include "Logger.h"

#include <iomanip>
#include <map>

using std::ios;
using std::map;
using std::ostringstream;

long State::m_reopenTime = 0;
int State::m_lockCounter = 0;
int State::m_lockRetries = 0;
Sequence State::m_sequence;
EbusMessage* State::m_activeMessage = nullptr;
EbusMessage* State::m_passiveMessage = nullptr;

map<int, string> StateErros =
{
{ STATE_INF_PRI_FIT, "priority class fit -> retry" },

{ STATE_WRN_BYTE_DIF, "written/read byte difference" },
{ STATE_WRN_ARB_LOST, "arbitration lost" },
{ STATE_WRN_PRI_LOST, "priority class lost" },
{ STATE_WRN_ACK_NEG, "received ACK is negative -> retry" },
{ STATE_WRN_RECV_RESP, "received response is invalid -> retry" },
{ STATE_WRN_RECV_MSG, "at me addressed message is invalid" },
{ STATE_WRN_NOT_DEF, "at me addressed message is not defined" },

{ STATE_ERR_LOCK_FAIL, "lock ebus failed" },
{ STATE_ERR_ACK_NEG, "received ACK is negative -> failed" },
{ STATE_ERR_ACK_WRONG, "received ACK byte is wrong" },
{ STATE_ERR_NN_WRONG, "received NN byte is wrong" },
{ STATE_ERR_RECV_RESP, "received response is invalid -> failed" },
{ STATE_ERR_CREA_MSG, "creating the message failed" },
{ STATE_ERR_SEND_FAIL, "sending the response message failed" } };

State::~State()
{
}

void State::changeState(EbusHandler* h, State* state)
{
	h->changeState(state);
}

int State::read(EbusHandler* h, unsigned char& byte, const long sec, const long nsec)
{
	Logger L = Logger("State::read");

	int result = h->m_device->recv(byte, sec, nsec);

	if (h->m_logRaw == true && result == DEV_OK) L.log(info, "<%02x", byte);

	if (h->m_dumpRaw == true && result == DEV_OK && h->m_dumpRawStream.is_open() == true)
	{
		h->m_dumpRawStream.write((char*) &byte, 1);
		h->m_dumpRawFileSize++;

		if ((h->m_dumpRawFileSize % 8) == 0) h->m_dumpRawStream.flush();

		if (h->m_dumpRawFileSize >= h->m_dumpRawFileMaxSize * 1024)
		{
			string oldfile = h->m_dumpRawFile + ".old";

			if (rename(h->m_dumpRawFile.c_str(), oldfile.c_str()) == 0)
			{
				h->m_dumpRawStream.close();
				h->m_dumpRawStream.open(h->m_dumpRawFile.c_str(), ios::binary | ios::app);
				h->m_dumpRawFileSize = 0;
			}
		}
	}

	return (result);
}

int State::write(EbusHandler* h, const unsigned char& byte)
{
	Logger L = Logger("State::write");

	int result = h->m_device->send(byte);

	if (h->m_logRaw == true && result == DEV_OK) L.log(info, ">%02x", byte);

	return (result);
}

int State::writeRead(EbusHandler* h, const unsigned char& byte, const long timeout)
{
	Logger L = Logger("State::writeRead");

	int result = State::write(h, byte);

	if (result == DEV_OK)
	{
		unsigned char readByte;
		result = State::read(h, readByte, 0, timeout);

		if (readByte != byte) L.log(trace, "%s", errorText(STATE_WRN_BYTE_DIF).c_str());
	}

	return (result);
}

void State::reset(EbusHandler* h)
{
	m_reopenTime = 0;
	m_lockCounter = h->m_lockCounter;
	m_lockRetries = 0;
	m_sequence.clear();

	if (m_activeMessage != nullptr)
	{
		m_activeMessage->notify();

		if (h->m_activeDone == false)
		{
			EbusMessage* ebusMessage = m_activeMessage;
			delete ebusMessage;
			h->m_activeDone = true;
		}

		m_activeMessage = nullptr;
	}

	if (m_passiveMessage != nullptr)
	{
		EbusMessage* tmp = m_passiveMessage;
		m_passiveMessage = nullptr;
		delete tmp;
	}
}

const string State::errorText(const int error)
{
	ostringstream errStr;

	if (error == 0)
		errStr << color::green << StateErros[error];
	else if (error > 0)
		errStr << color::yellow << StateErros[error];
	else
		errStr << color::red << StateErros[error];

	errStr << color::reset;

	return (errStr.str());
}

