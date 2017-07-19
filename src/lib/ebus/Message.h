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

#ifndef LIBEBUS_MESSAGE_H
#define LIBEBUS_MESSAGE_H

#include "EbusSequence.h"
#include "Notify.h"

using libutils::Notify;

namespace libebus
{

class Message : public Notify
{

public:
	explicit Message(EbusSequence& eSeq);

	EbusSequence& getEbusSequence();

	void setState(int state);
	int getState();

private:
	EbusSequence& m_ebusSequence;
	int m_state = 0;

};

} // namespace libebus

#endif // LIBEBUS_MESSAGE_H
