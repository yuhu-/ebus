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

#ifndef UTILS_NQUEUE_H
#define UTILS_NQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

using std::queue;
using std::mutex;
using std::lock_guard;
using std::unique_lock;
using std::condition_variable;

template<typename T>
class NQueue
{

public:
	NQueue()
		: m_queue(), m_mutex(), m_cond()
	{
	}

	void enqueue(T item)
	{
		lock_guard<mutex> lock(m_mutex);
		m_queue.push(item);
		m_cond.notify_one();
	}

	T dequeue()
	{
		unique_lock<mutex> lock(m_mutex);
		while (m_queue.empty() == true)
			m_cond.wait(lock);

		T val = m_queue.front();
		m_queue.pop();
		return (val);
	}

	size_t size()
	{
		unique_lock<mutex> lock(m_mutex);
		return (m_queue.size());
	}

private:
	queue<T> m_queue;
	mutex m_mutex;
	condition_variable m_cond;

};

#endif // UTILS_NQUEUE_H