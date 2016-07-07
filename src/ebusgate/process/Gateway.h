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

#ifndef PROCESS_GATEWAY_H
#define PROCESS_GATEWAY_H

#include "Process.h"

class Gateway : public Process
{

public:
	Gateway(const unsigned char address, const bool forward);

	ActionType active(EbusSequence& eSeq);

	void passive(EbusSequence& eSeq);

private:
	void run();

};

#endif // PROCESS_GATEWAY_H
