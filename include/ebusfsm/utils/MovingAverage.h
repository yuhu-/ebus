/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#ifndef EBUSFSM_UTILS_MOVINGAVERAGE_H
#define EBUSFSM_UTILS_MOVINGAVERAGE_H

#include <queue>

namespace ebusfsm
{

class MovingAverage
{

public:
	MovingAverage(const size_t size)
		: m_size(size), m_values()
	{
	}

	~MovingAverage()
	{
		std::queue<double>().swap(m_values);
	}

	void addValue(const double value)
	{
		if (m_values.size() >= m_size)
		{
			double oldestValue = m_values.front();
			m_values.pop();

			m_values.push(value);
			m_average += (value - oldestValue) / m_size;
		}
		else
		{
			double average = m_values.size() * m_average;
			m_values.push(value);
			m_average = (average + value) / m_values.size();
		}
	}

	double getAverage() const
	{
		return (m_average);
	}

private:
	const size_t m_size;
	double m_average = 0;
	std::queue<double> m_values;

};

} // namespace ebusfsm

#endif // EBUSFSM_UTILS_MOVINGAVERAGE_H
