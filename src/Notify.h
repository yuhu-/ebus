/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#ifndef EBUS_NOTIFY_H
#define EBUS_NOTIFY_H

#include <condition_variable>
#include <mutex>

namespace ebus
{

class Notify
{

public:
	Notify() : m_mutex(), m_condition()
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

} // namespace ebus

#endif // EBUS_NOTIFY_H
