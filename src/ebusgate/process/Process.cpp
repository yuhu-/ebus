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

#include "Process.h"
#include "Logger.h"

#include <iomanip>

using std::ostringstream;
using std::endl;

Process::Process(const unsigned char address, const bool forward)
	: Notify(), m_address(address)
{
	if (forward == true)
	{
		m_forward = new Forward();
		m_forward->start();
	}
}

Process::~Process()
{
	if (m_forward != nullptr)
	{
		m_forward->stop();
		delete m_forward;
		m_forward = nullptr;
	}
}

void Process::start()
{
	m_thread = thread(&Process::run, this);
}

void Process::stop()
{
	if (m_thread.joinable())
	{
		m_running = false;
		notify();
		m_thread.join();
	}
}

void Process::forward(bool remove, const string& ip, long port, const string& filter, ostringstream& result)
{
	if (remove == true)
		m_forward->remove(ip, port, filter, result);
	else
		m_forward->append(ip, port, filter, result);
}


