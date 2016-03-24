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

#ifndef LIBUTILS_NOTIFY_H
#define LIBUTILS_NOTIFY_H

#include <mutex>
#include <condition_variable>

using std::mutex;
using std::lock_guard;
using std::unique_lock;
using std::condition_variable;

class Notify
{

public:
	Notify()
		: m_notify(false), m_mutex(), m_cond()
	{
	}

	void waitNotify()
	{
		unique_lock<mutex> lock(m_mutex);
		while (m_notify == false)
		{
			m_cond.wait(lock);
			m_notify = false;
			break;
		}
	}

	void notify()
	{
		lock_guard<mutex> lock(m_mutex);
		m_notify = true;
		m_cond.notify_one();
	}

private:
	mutex m_mutex;
	condition_variable m_cond;

	bool m_notify;

};

#endif // LIBUTILS_NOTIFY_H
