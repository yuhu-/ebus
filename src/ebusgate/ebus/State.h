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

#ifndef EBUS_STATE_H
#define EBUS_STATE_H

#include "EbusHandler.h"

#define STATE_INF_PRI_FIT     0 // priority class fit -> retry

#define STATE_WRN_BYTE_DIF    1 // written/read byte difference
#define STATE_WRN_ARB_LOST    2 // arbitration lost
#define STATE_WRN_PRI_LOST    3 // priority class lost
#define STATE_WRN_ACK_NEG     4 // received ACK is negative -> retry
#define STATE_WRN_RECV_RESP   5 // received response is invalid -> retry
#define STATE_WRN_RECV_MESS   6 // to me addressed message is invalid

#define STATE_ERR_LOCK_FAIL  -1 // lock ebus failed
#define STATE_ERR_ACK_NEG    -2 // received ACK is negative -> failed
#define STATE_ERR_ACK_WRONG  -3 // received ACK byte is wrong
#define STATE_ERR_RECV_RESP  -4 // received response is invalid -> failed
#define STATE_ERR_SEND_FAIL  -5 // sending response failed

class State
{

public:
	virtual int run(EbusHandler* h) = 0;
	virtual const char* toString() const = 0;

protected:
	virtual ~State();

	static long m_reopenTime;
	static int m_lockCounter;
	static int m_lockRetries;
	static Sequence m_sequence;
	static EbusMessage* m_ebusMessage;

	static void changeState(EbusHandler* h, State* state);
	static int read(EbusHandler* h, unsigned char& byte, const long sec,
		const long nsec);
	static int write(EbusHandler* h, const unsigned char& byte);
	static int writeRead(EbusHandler* h, const unsigned char& byte,
		const long timeout);

	static void reset(EbusHandler* h);

	static const string errorText(const int error);
};

#endif // EBUS_STATE_H
