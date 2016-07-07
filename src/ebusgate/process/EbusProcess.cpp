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

#include "EbusProcess.h"

#include "Gateway.h"
#include "Logger.h"

EbusProcess::EbusProcess(const ProcessType type, const unsigned char address, const bool forward)
{
	if (type == pt_gateway) m_process = new Gateway(address, forward);

	if (m_process != nullptr) m_process->start();
}

EbusProcess::~EbusProcess()
{
	if (m_process != nullptr)
	{
		m_process->stop();
		delete m_process;
		m_process = nullptr;
	}
}

void EbusProcess::forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result)
{
	m_process->forward(remove, ip, port, filter, result);
}

Process* EbusProcess::getProcess()
{
	return (m_process);
}

