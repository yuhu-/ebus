/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
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

#include <State.h>
#include <EbusFSM.h>
#include <utils/Color.h>

#include <iomanip>
#include <map>

long ebusfsm::State::m_reopenTime = 0;
int ebusfsm::State::m_lockCounter = 0;
int ebusfsm::State::m_lockRetries = 0;
ebusfsm::Sequence ebusfsm::State::m_sequence;
ebusfsm::Message* ebusfsm::State::m_activeMessage = nullptr;
ebusfsm::Message* ebusfsm::State::m_passiveMessage = nullptr;

std::map<int, std::string> StateMessages =
{
{ STATE_INF_EBUS_ON, "ebus connected" },
{ STATE_INF_EBUS_OFF, "ebus disconnected" },
{ STATE_INF_EBUS_LOCK, "ebus locked" },
{ STATE_INF_EBUS_FREE, "ebus freed" },
{ STATE_INF_MSG_INGORE, "message ignored" },
{ STATE_INF_DEV_FLUSH, "device flushed" },
{ STATE_INF_NOT_DEF, "at me addressed message is undefined" },
{ STATE_INF_NO_FUNC, "function not implemented" },

{ STATE_WRN_BYTE_DIF, "written/read byte difference" },
{ STATE_WRN_ARB_LOST, "arbitration lost" },
{ STATE_WRN_PRI_FIT, "priority class fit -> retry" },
{ STATE_WRN_PRI_LOST, "priority class lost" },
{ STATE_WRN_ACK_NEG, "received acknowledge byte is negative -> retry" },
{ STATE_WRN_RECV_RESP, "received response is invalid -> retry" },
{ STATE_WRN_RECV_MSG, "at me addressed message is invalid" },

{ STATE_ERR_LOCK_FAIL, "lock ebus failed" },
{ STATE_ERR_ACK_NEG, "received acknowledge byte is negative -> failed" },
{ STATE_ERR_ACK_WRONG, "received acknowledge byte is wrong" },
{ STATE_ERR_NN_WRONG, "received size byte is wrong" },
{ STATE_ERR_RECV_RESP, "received response is invalid -> failed" },
{ STATE_ERR_RESP_CREA, "creating response message failed" },
{ STATE_ERR_RESP_SEND, "sending response message failed" },
{ STATE_ERR_BAD_TYPE, "received message type does not allow an answer" } };

ebusfsm::State::~State()
{
}

void ebusfsm::State::changeState(EbusFSM* fsm, State* state)
{
	fsm->changeState(state);
}

int ebusfsm::State::read(EbusFSM* fsm, unsigned char& byte, const long sec, const long nsec)
{
	int result = fsm->m_ebusDevice->recv(byte, sec, nsec);

	if (result == DEV_OK)
	{
		std::ostringstream ostr;
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte) << std::nouppercase << std::setw(0);
		fsm->logTrace("<" + ostr.str());
	}

	if (fsm->m_dump == true && result == DEV_OK && fsm->m_dumpRawStream.is_open() == true)
	{
		fsm->m_dumpRawStream.write((char*) &byte, 1);
		fsm->m_dumpFileSize++;

		if ((fsm->m_dumpFileSize % 8) == 0) fsm->m_dumpRawStream.flush();

		if (fsm->m_dumpFileSize >= fsm->m_dumpFileMaxSize * 1024)
		{
			std::string oldfile = fsm->m_dumpFile + ".old";

			if (rename(fsm->m_dumpFile.c_str(), oldfile.c_str()) == 0)
			{
				fsm->m_dumpRawStream.close();
				fsm->m_dumpRawStream.open(fsm->m_dumpFile.c_str(), std::ios::binary | std::ios::app);
				fsm->m_dumpFileSize = 0;
			}
		}
	}

	return (result);
}

int ebusfsm::State::write(EbusFSM* fsm, const unsigned char& byte)
{
	int result = fsm->m_ebusDevice->send(byte);

	if (result == DEV_OK)
	{
		std::ostringstream ostr;
		ostr << std::nouppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte) << std::nouppercase << std::setw(0);
		fsm->logTrace(">" + ostr.str());
	}
	return (result);
}

int ebusfsm::State::writeRead(EbusFSM* fsm, const unsigned char& byte, const long sec, const long nsec)
{
	int result = State::write(fsm, byte);

	if (result == DEV_OK)
	{
		unsigned char readByte;
		result = State::read(fsm, readByte, sec, nsec);

		if (readByte != byte) fsm->logDebug(stateMessage(fsm, STATE_WRN_BYTE_DIF));
	}

	return (result);
}

void ebusfsm::State::reset(EbusFSM* fsm)
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

const std::string ebusfsm::State::stateMessage(EbusFSM* fsm, const int state)
{
	std::ostringstream ostr;

	if (fsm->m_color == true)
	{
		if (state < 11)
			ostr << color::green;
		else if (state < 21)
			ostr << color::yellow;
		else
			ostr << color::red;

		ostr << StateMessages[state] << color::reset;
	}
	else
	{
		ostr << StateMessages[state];
	}

	return (ostr.str());
}

const std::string ebusfsm::State::eSeqMessage(EbusFSM* fsm, EbusSequence& eSeq)
{
	std::ostringstream ostr;

	if (fsm->m_color == true)
	{
		if (eSeq.getMasterState() == SEQ_OK)
		{
			if (eSeq.getType() == SEQ_TYPE_BC)
			{
				ostr << color::blue << "BC" << color::reset << " " << eSeq.toStringMaster();
			}
			else if (eSeq.getType() == SEQ_TYPE_MM)
			{
				ostr << color::cyan << "MM" << color::reset << " " << eSeq.toStringMaster() << " "
					<< eSeq.toStringSlaveACK();
			}
			else
			{
				ostr << color::magenta << "MS" << color::reset << " " << eSeq.toStringMaster();

				if (eSeq.getSlaveState() == SEQ_OK)
					ostr << " " << eSeq.toStringSlave();
				else
					ostr << " " << color::red << eSeq.toStringSlave() << color::reset;
			}

		}
		else
		{
			ostr << color::red << eSeq.toStringMaster() << color::reset;
		}

	}
	else
	{
		if (eSeq.getMasterState() == SEQ_OK)
		{
			if (eSeq.getType() == SEQ_TYPE_BC)
				ostr << "BC ";
			else if (eSeq.getType() == SEQ_TYPE_MM)
				ostr << "MM ";
			else
				ostr << "MS ";
		}

		ostr << eSeq.toString();
	}

	return (ostr.str());
}