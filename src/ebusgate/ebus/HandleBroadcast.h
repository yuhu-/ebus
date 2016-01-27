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

#ifndef EBUS_HANDLEBROADCAST_H
#define EBUS_HANDLEBROADCAST_H

#include "State.h"

class HandleBroadcast : public State
{

public:
	static HandleBroadcast* getInstance()
	{
		return (&m_handleBroadcast);
	}

	int run(EbusHandler* h);
	const char* toString() const;

private:
	HandleBroadcast();
	static HandleBroadcast m_handleBroadcast;

	static int needAction(const EbusSequence& eSeq);
	static bool createMessage(const unsigned char source,
		const unsigned char target, EbusSequence& eSeq);

};

#endif // EBUS_HANDLEBROADCAST_H
