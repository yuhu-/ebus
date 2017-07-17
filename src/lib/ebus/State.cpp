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

#include "State.h"
#include "EbusFSM.h"
#include "Color.h"

#include <iomanip>
#include <map>

using std::ios;
using std::map;
using std::ostringstream;
using std::nouppercase;
using std::hex;
using std::setw;
using std::setfill;

long libebus::State::m_reopenTime = 0;
int libebus::State::m_lockCounter = 0;
int libebus::State::m_lockRetries = 0;
libebus::Sequence libebus::State::m_sequence;
libebus::Message* libebus::State::m_activeMessage = nullptr;
libebus::Message* libebus::State::m_passiveMessage = nullptr;

map<int, string> StateMessages =
{
{ STATE_INF_EBUS_ON, "ebus connected" },
{ STATE_INF_EBUS_OFF, "ebus disconnected" },
{ STATE_INF_EBUS_LOCK, "ebus locked" },
{ STATE_INF_EBUS_FREE, "ebus freed" },
{ STATE_INF_MSG_INGORE, "message ignored" },
{ STATE_INF_DEV_FLUSH, "device flushed" },

{ STATE_WRN_BYTE_DIF, "written/read byte difference" },
{ STATE_WRN_ARB_LOST, "arbitration lost" },
{ STATE_WRN_PRI_FIT, "priority class fit -> retry" },
{ STATE_WRN_PRI_LOST, "priority class lost" },
{ STATE_WRN_ACK_NEG, "received ACK byte is negative -> retry" },
{ STATE_WRN_RECV_RESP, "received response is invalid -> retry" },
{ STATE_WRN_RECV_MSG, "at me addressed message is invalid" },
{ STATE_WRN_NOT_DEF, "at me addressed message is undefined" },
{ STATE_WRN_NO_FUNC, "function not implemented" },

{ STATE_ERR_LOCK_FAIL, "lock ebus failed" },
{ STATE_ERR_ACK_NEG, "received ACK byte is negative -> failed" },
{ STATE_ERR_ACK_WRONG, "received ACK byte is wrong" },
{ STATE_ERR_NN_WRONG, "received NN byte is wrong" },
{ STATE_ERR_RECV_RESP, "received response is invalid -> failed" },
{ STATE_ERR_RESP_CREA, "creating response message failed" },
{ STATE_ERR_RESP_SEND, "sending response message failed" } };

libebus::State::~State()
{
}

void libebus::State::changeState(EbusFSM* fsm, State* state)
{
	fsm->changeState(state);
}

int libebus::State::read(EbusFSM* fsm, unsigned char& byte, const long sec, const long nsec)
{
	int result = fsm->m_ebusDevice->recv(byte, sec, nsec);

	if (result == DEV_OK)
	{
		ostringstream ostr;
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(byte) << nouppercase
			<< setw(0);
		fsm->logTrace("<" + ostr.str());
	}

	if (fsm->m_dump == true && result == DEV_OK && fsm->m_dumpRawStream.is_open() == true)
	{
		fsm->m_dumpRawStream.write((char*) &byte, 1);
		fsm->m_dumpFileSize++;

		if ((fsm->m_dumpFileSize % 8) == 0) fsm->m_dumpRawStream.flush();

		if (fsm->m_dumpFileSize >= fsm->m_dumpFileMaxSize * 1024)
		{
			string oldfile = fsm->m_dumpFile + ".old";

			if (rename(fsm->m_dumpFile.c_str(), oldfile.c_str()) == 0)
			{
				fsm->m_dumpRawStream.close();
				fsm->m_dumpRawStream.open(fsm->m_dumpFile.c_str(), ios::binary | ios::app);
				fsm->m_dumpFileSize = 0;
			}
		}
	}

	return (result);
}

int libebus::State::write(EbusFSM* fsm, const unsigned char& byte)
{
	int result = fsm->m_ebusDevice->send(byte);

	if (result == DEV_OK)
	{
		ostringstream ostr;
		ostr << nouppercase << hex << setw(2) << setfill('0') << static_cast<unsigned>(byte) << nouppercase
			<< setw(0);
		fsm->logTrace(">" + ostr.str());
	}
	return (result);
}

int libebus::State::writeRead(EbusFSM* fsm, const unsigned char& byte, const long timeout)
{
	int result = State::write(fsm, byte);

	if (result == DEV_OK)
	{
		unsigned char readByte;
		result = State::read(fsm, readByte, 0, timeout);

		if (readByte != byte) fsm->logDebug(stateMessage(STATE_WRN_BYTE_DIF));
	}

	return (result);
}

void libebus::State::reset(EbusFSM* fsm)
{
	m_reopenTime = 0;
	m_lockCounter = fsm->m_lockCounter;
	m_lockRetries = 0;
	m_sequence.clear();

	if (m_activeMessage != nullptr)
	{
		fsm->publish(m_activeMessage->getEbusSequence());
		m_activeMessage->notify();
		m_activeMessage = nullptr;
	}

	if (m_passiveMessage != nullptr)
	{
		Message* message = m_passiveMessage;
		m_passiveMessage = nullptr;
		delete message;
	}
}

const string libebus::State::stateMessage(const int state)
{
	ostringstream ostr;

	if (state < 11)
		ostr << libutils::color::green << StateMessages[state];
	else if (state < 21)
		ostr << libutils::color::yellow << StateMessages[state];
	else
		ostr << libutils::color::red << StateMessages[state];

	ostr << libutils::color::reset;

	return (ostr.str());
}
