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

#ifndef PROCESS_DUMMY_H
#define PROCESS_DUMMY_H

#include "Process.h"

class Dummy : public Process
{

public:
	explicit Dummy(const unsigned char address);
	~Dummy();

	Action handleActive(EbusSequence& eSeq);

	void activeSent(EbusSequence& eSeq);

	void handlePassive(EbusSequence& eSeq);

private:
	void run();

};

#endif // PROCESS_DUMMY_H
