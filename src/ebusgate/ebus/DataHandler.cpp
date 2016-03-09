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

#include "DataHandler.h"
#include "Logger.h"

#include <iomanip>
#include <algorithm>

using std::make_tuple;
using std::ostringstream;
using std::endl;
using std::get;
using std::remove;

int DataHandler::hostID = 1;
int DataHandler::filterID = 1;

DataHandler::~DataHandler()
{
	while (m_ebusDataQueue.size() > 0)
		delete m_ebusDataQueue.dequeue();
}

void DataHandler::start()
{
	m_thread = thread(&DataHandler::run, this);
}

void DataHandler::stop()
{
	if (m_thread.joinable())
	{
		m_running = false;
		notify();
		m_thread.join();
	}
}

bool DataHandler::subscribe(const string& ip, const long& port, const string& filter, ostringstream& result)
{
	tuple<string, long> ipPort = make_tuple(ip, port);
	bool oldHost = true;
	size_t hostIndex;

	for (hostIndex = 0; hostIndex < m_hosts.size(); hostIndex++)
		if (m_hosts[hostIndex].ipPort == ipPort) break;

	if (hostIndex == m_hosts.size())
	{
		Host newHost;

		newHost.ipPort = ipPort;
		newHost.id = hostID++;
		newHost.filter = (not filter.empty());

		m_hosts.push_back(newHost);

		oldHost = false;
	}

	if (filter.empty() == true)
	{
		if (m_hosts[hostIndex].filter == false)
		{
			if (oldHost == false)
				result << "host added";
			else
				result << "host skipped - already subscribed";

			return (true);
		}

		vector<tuple<int, int>> delHostFilter;

		for (const auto& hostFilter : m_hostFilter)
			if (get<0>(hostFilter) == m_hosts[hostIndex].id) delHostFilter.push_back(hostFilter);

		for (const auto& delHost : delHostFilter)
			m_hostFilter.erase(remove(m_hostFilter.begin(), m_hostFilter.end(), delHost),
				m_hostFilter.end());

		vector<int> filterIDs;

		for (const auto& oldFilter : m_filter)
		{
			bool found = false;

			for (const auto& hostFilter : m_hostFilter)
				if (get<1>(hostFilter) == oldFilter.id)
				{
					found = true;
					break;
				}

			if (found == false) filterIDs.push_back(oldFilter.id);
		}

		for (const auto& filtID : filterIDs)
		{
			for (auto it = m_filter.cbegin(); it != m_filter.cend(); it++)
				if (it->id == filtID)
				{
					m_filter.erase(it);
					break;
				}
		}

		m_hosts[hostIndex].filter = false;
		result << "filter entries for this host deleted";
		return (true);
	}

	if (m_hosts[hostIndex].filter == false && oldHost == true)
	{
		result << "filter skipped - host without filter already subscribed";
		return (true);
	}

	Sequence seq(filter);
	size_t filtIndex;

	for (filtIndex = 0; filtIndex < m_filter.size(); filtIndex++)
		if (seq.compare(m_filter[filtIndex].seq) == true) break;

	if (filtIndex == m_filter.size())
	{
		Filter newFilter;

		newFilter.seq = seq;
		newFilter.id = filterID++;

		m_filter.push_back(newFilter);
	}

	for (const auto& hostFilter : m_hostFilter)
		if (get<0>(hostFilter) == m_hosts[hostIndex].id && get<1>(hostFilter) == m_filter[filtIndex].id)
		{
			result << "host and filter already subscribed";
			return (true);
		}

	m_hostFilter.push_back(make_tuple(m_hosts[hostIndex].id, m_filter[filtIndex].id));

	result << "host and filter added";

	return (true);
}

bool DataHandler::unsubscribe(const string& ip, const long& port, const string& filter, ostringstream& result)
{

	tuple<string, long> ipPort = make_tuple(ip, port);
	size_t hostIndex;

	for (hostIndex = 0; hostIndex < m_hosts.size(); hostIndex++)
		if (m_hosts[hostIndex].ipPort == ipPort) break;

	if (hostIndex == m_hosts.size())
	{
		result << "host not found";
		return (true);
	}

	int countFilter = 0;
	for (const auto& hostFilter : m_hostFilter)
		if (get<0>(hostFilter) == m_hosts[hostIndex].id) countFilter++;

	if (filter.empty() == true || countFilter <= 1)
	{
		vector<tuple<int, int>> delHostFilter;

		for (const auto& hostFilter : m_hostFilter)
			if (get<0>(hostFilter) == m_hosts[hostIndex].id) delHostFilter.push_back(hostFilter);

		for (const auto& delHost : delHostFilter)
			m_hostFilter.erase(remove(m_hostFilter.begin(), m_hostFilter.end(), delHost),
				m_hostFilter.end());

		vector<int> filterIDs;

		for (const auto& oldFilter : m_filter)
		{
			bool found = false;

			for (const auto& hostFilter : m_hostFilter)
				if (get<1>(hostFilter) == oldFilter.id)
				{
					found = true;
					break;
				}

			if (found == false) filterIDs.push_back(oldFilter.id);
		}

		for (const auto& filtID : filterIDs)
		{
			for (auto it = m_filter.cbegin(); it != m_filter.cend(); it++)
				if (it->id == filtID)
				{
					m_filter.erase(it);
					break;
				}
		}

		m_hosts.erase(m_hosts.begin() + hostIndex);

		result << "host and filter entries deleted";
		return (true);
	}

	tuple<int, int> delHostFilter;
	Sequence seq(filter);
	size_t filtIndex;

	for (filtIndex = 0; filtIndex < m_filter.size(); filtIndex++)
		if (seq.compare(m_filter[filtIndex].seq) == true) break;

	if (filtIndex == m_filter.size())
	{
		result << "filter not found";
		return (true);
	}

	for (const auto& hostFilter : m_hostFilter)
		if (get<0>(hostFilter) == m_hosts[hostIndex].id && get<1>(hostFilter) == m_filter[filtIndex].id)
			delHostFilter = hostFilter;

	m_hostFilter.erase(remove(m_hostFilter.begin(), m_hostFilter.end(), delHostFilter), m_hostFilter.end());

	countFilter = 0;
	for (const auto& hostFilter : m_hostFilter)
		if (get<1>(hostFilter) == m_filter[filtIndex].id) countFilter++;

	if (countFilter == 0)
	{
		for (auto it = m_filter.cbegin(); it != m_filter.cend(); it++)
			if (it->id == m_filter[filtIndex].id)
			{
				m_filter.erase(it);
				break;
			}
	}

	result << "filter deleted";
	return (true);
}

const string DataHandler::toString()
{
	ostringstream ostr;

	ostr << endl << toStringHosts();

	if (m_filter.empty() == false) ostr << endl << toStringFilters();

	if (m_hostFilter.empty() == false) ostr << endl << toStringHostFilter();

	ostr << endl;

	return (ostr.str());
}

const string DataHandler::toStringHosts()
{
	ostringstream ostr;

	for (Host host : m_hosts)
		ostr << get<0>(host.ipPort) << " " << get<1>(host.ipPort) << " " << host.id << " " << host.filter
			<< endl;

	return (ostr.str());
}

const string DataHandler::toStringFilters()
{
	ostringstream ostr;

	for (Filter filter : m_filter)
		ostr << filter.seq.toString() << " " << filter.id << endl;

	return (ostr.str());
}

const string DataHandler::toStringHostFilter()
{
	ostringstream ostr;

	for (const auto& hostFilter : m_hostFilter)
		ostr << get<0>(hostFilter) << " " << get<1>(hostFilter) << endl;

	return (ostr.str());
}

void DataHandler::enqueue(const EbusSequence& eSeq)
{
	if (m_filter.empty() == false)
	{
		m_ebusDataQueue.enqueue(new EbusSequence(eSeq));
		notify();
	}
}

void DataHandler::run()
{
	Logger logger = Logger("DataHandler::run");

	while (m_running == true)
	{
		waitNotify();
		if (m_ebusDataQueue.size() > 0)
		{
			EbusSequence* eSeq = m_ebusDataQueue.dequeue();
			logger.trace("%s", eSeq->toString().c_str());
			Sequence seq = eSeq->getMaster();

			for (auto& filter : m_filter)
			{
				if (seq.search(filter.seq) >= 0)
				{
					logger.debug("found: %s in %s (%d)", filter.seq.toString().c_str(),
						seq.toString().c_str(), filter.id);
					//todo send :-)
				}
			}

			delete eSeq;
		}
	}
}
