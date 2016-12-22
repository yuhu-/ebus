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

#ifndef PROCESS_PROXY_H
#define PROCESS_PROXY_H

#include "Process.h"

class Proxy : public Process
{

public:
	explicit Proxy(const unsigned char address);
	~Proxy();

	Action active(EbusSequence& eSeq);

	void passive(EbusSequence& eSeq);

	void forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result);

private:
	Forward* m_forward = nullptr;

	void run();

};

#endif // PROCESS_PROXY_H
