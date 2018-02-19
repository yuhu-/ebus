/*
 * Copyright (C) Roland Jax 2012-2018 <roland.jax@liwest.at>
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

#ifndef EBUSFSM_UTILS_NOTIFY_H
#define EBUSFSM_UTILS_NOTIFY_H

#include <mutex>
#include <condition_variable>

namespace ebusfsm
{

class Notify
{

public:
	Notify()
		: m_mutex(), m_condition()
	{
	}

	void waitNotify()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (m_notify == false)
		{
			m_condition.wait(lock);
			m_notify = false;
			break;
		}
	}

	void notify()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_notify = true;
		m_condition.notify_one();
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_condition;
	bool m_notify = false;

};

} // namespace ebusfsm

#endif // EBUSFSM_UTILS_NOTIFY_H
