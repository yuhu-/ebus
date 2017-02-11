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

#ifndef PROCESS_PROXY_H
#define PROCESS_PROXY_H

#include "EbusProcess.h"
#include "Forward.h"

class Proxy : public EbusProcess
{

public:
	explicit Proxy(const unsigned char address);
	~Proxy();

	void forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result);

private:
	unique_ptr<Forward> m_forward = nullptr;

	void run();

	Action getEvaluatedAction(EbusSequence& eSeq);
	void evalActiveMessage(EbusSequence& eSeq);
	void evalPassiveMessage(EbusSequence& eSeq);

};

#endif // PROCESS_PROXY_H
