/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#ifndef EBUSFSM_SENDMESSAGE_H
#define EBUSFSM_SENDMESSAGE_H

#include <State.h>

namespace ebusfsm
{

class SendMessage : public State
{

public:
	static SendMessage* getSendMessage()
	{
		return (&m_sendMessage);
	}

	int run(EbusFSM* fsm) override;
	const std::string toString() const override;

private:
	static SendMessage m_sendMessage;

};

} // namespace ebusfsm

#endif // EBUSFSM_SENDMESSAGE_H
