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

#ifndef EBUS_FSM_EVALUATE_H
#define EBUS_FSM_EVALUATE_H

#include "../../../ebuscpp/ebus/fsm/State.h"

class Evaluate : public State
{

public:
	static Evaluate* getEvaluate()
	{
		return (&m_evaluate);
	}

	int run(EbusFSM* fsm);
	const string toString() const;

private:
	static Evaluate m_evaluate;

};

#endif // EBUS_FSM_EVALUATE_H
