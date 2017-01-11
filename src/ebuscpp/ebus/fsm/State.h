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

#ifndef EBUS_FSM_STATE_H
#define EBUS_FSM_STATE_H

#include "EbusFSM.h"

#define STATE_INF_EBUS_ON      1 // ebus connected
#define STATE_INF_EBUS_OFF     2 // ebus disconnected
#define STATE_INF_EBUS_LOCK    3 // ebus locked
#define STATE_INF_EBUS_FREE    4 // ebus freed

#define STATE_WRN_BYTE_DIF    11 // written/read byte difference
#define STATE_WRN_ARB_LOST    12 // arbitration lost
#define STATE_WRN_PRI_FIT     13 // priority class fit -> retry
#define STATE_WRN_PRI_LOST    14 // priority class lost
#define STATE_WRN_ACK_NEG     15 // received ACK is negative -> retry
#define STATE_WRN_RECV_RESP   16 // received response is invalid -> retry
#define STATE_WRN_RECV_MSG    17 // at me addressed message is invalid
#define STATE_WRN_NOT_DEF     18 // at me addressed message is not defined
#define STATE_WRN_NO_PROCESS  19 // process not implemented

#define STATE_ERR_LOCK_FAIL   21 // lock ebus failed
#define STATE_ERR_ACK_NEG     22 // received ACK is negative -> failed
#define STATE_ERR_ACK_WRONG   23 // received ACK byte is wrong
#define STATE_ERR_NN_WRONG    24 // received NN byte is wrong
#define STATE_ERR_RECV_RESP   25 // received response is invalid -> failed
#define STATE_ERR_CREA_MSG    26 // creating the message failed
#define STATE_ERR_SEND_FAIL   27 // sending the response message failed

class State
{

public:
	virtual int run(EbusFSM* fsm) = 0;
	virtual const string toString() const = 0;

protected:
	virtual ~State();

	static long m_reopenTime;
	static int m_lockCounter;
	static int m_lockRetries;
	static Sequence m_sequence;
	static EbusMessage* m_activeMessage;
	static EbusMessage* m_passiveMessage;

	static void changeState(EbusFSM* fsm, State* state);
	static int read(EbusFSM* fsm, unsigned char& byte, const long sec, const long nsec);
	static int write(EbusFSM* fsm, const unsigned char& byte);
	static int writeRead(EbusFSM* fsm, const unsigned char& byte, const long timeout);

	static void reset(EbusFSM* fsm);

	static const string stateMessage(const int state);
};

#endif // EBUS_FSM_STATE_H
