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

#ifndef PROCESS_IPROCESS_H
#define PROCESS_IPROCESS_H

#include "EbusSequence.h"

enum ActionType
{
	at_undefined,	// undefined
	at_ignore,	// ignore
	at_response,	// send response
	at_send		// send message
};

class IProcess
{

public:
	virtual ~IProcess()
	{
	}

	virtual ActionType active(EbusSequence& eSeq) = 0;

	virtual void passive(EbusSequence& eSeq) = 0;

};

#endif // PROCESS_IPROCESS_H
