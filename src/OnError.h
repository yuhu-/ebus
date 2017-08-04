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

#ifndef EBUSFSM_ONERROR_H
#define EBUSFSM_ONERROR_H

#include <State.h>

namespace ebusfsm
{

class OnError : public State
{

public:
	static OnError* getOnError()
	{
		return (&m_onError);
	}

	int run(EbusFSM* fsm);
	const std::string toString() const;

private:
	static OnError m_onError;

};

} // namespace ebusfsm

#endif // EBUSFSM_ONERROR_H
