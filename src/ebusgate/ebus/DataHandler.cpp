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

using std::ostringstream;
using std::endl;

DataHandler::DataHandler()
	: Notify()
{
}

DataHandler::~DataHandler()
{
	while (m_ebusDataQueue.size() > 0)
		delete m_ebusDataQueue.dequeue();

	while (m_host.size() > 0)
	{
		Host* host = m_host.back();
		m_host.pop_back();
		delete host;
	}

	while (m_filter.size() > 0)
	{
		Filter* filter = m_filter.back();
		m_filter.pop_back();
		delete filter;
	}

	while (m_relation.size() > 0)
	{
		Relation* relation = m_relation.back();
		m_relation.pop_back();
		delete relation;
	}
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

bool DataHandler::append(const string& ip, long port, const string& filter, ostringstream& result)
{
	Logger logger = Logger("DataHandler::append");

	Host* host = getHost(ip, port);

	// new host
	if (host == nullptr)
	{
		host = addHost(ip, port, (not filter.empty()));
		logger.debug("host %d added", host->getID());

		// host with filter
		if (host->hasFilter() == true)
		{
			const Filter* filt = getFilter(filter);

			// new filter
			if (filt == nullptr)
			{
				filt = addFilter(filter);
				logger.debug("filter %d added", filt->getID());
			}

			addRelation(host->getID(), filt->getID());
			logger.debug("relation [%d:%d] added", host->getID(), filt->getID());

			result << "host with filter subscribed";
		}

		// host without filter
		else
		{
			result << "host without filter subscribed";
		}
	}

	// old host
	else
	{
		// host with filter
		if (host->hasFilter() == true)
		{
			// no new filter
			if (filter.empty() == true)
			{
				host->setFilter(false);
				logger.debug("host %d filter updated", host->getID());

				delRelationByHost(host->getID());
				clrFilter();

				result << "host " << host->getID() << " filter updated, associated filter removed";
			}

			// new filter
			else
			{
				const Filter* filt = getFilter(filter);

				// new filter
				if (filt == nullptr)
				{
					filt = addFilter(filter);
					logger.debug("filter %d added", filt->getID());
				}

				// new relation
				if (getRelation(host->getID(), filt->getID()) == nullptr)
				{
					addRelation(host->getID(), filt->getID());
					logger.debug("relation [%d:%d] added", host->getID(), filt->getID());

					result << "filter to existing host added";
				}

				// old relation
				else
				{
					result << "host with this filter already subscribed, filter ignored";
				}
			}
		}

		// host without filter
		else
		{
			// no new filter
			if (filter.empty() == true)
				result << "host without filter already subscribed, host ignored";

			// new filter
			else
				result << "host without filter already subscribed, filter ignored";

		}
	}

	logger.info("%s", result.str().c_str());
	return (true);
}

bool DataHandler::remove(const string& ip, long port, const string& filter, ostringstream& result)
{
	Logger logger = Logger("DataHandler::remove");

	Host* host = getHost(ip, port);

	// host not found
	if (host == nullptr)
	{
		result << "host not found";
	}

	// host found
	else
	{
		// host with filter
		if (host->hasFilter() == true)
		{
			// without filter
			if (filter.empty() == true)
			{
				int hostID = delHost(ip, port);
				logger.debug("host %d removed", hostID);

				delRelationByHost(hostID);
				clrFilter();

				result << "host " << hostID << " removed, associated filter removed";
			}

			// with filter
			else
			{
				// filter not found
				if (getFilter(filter) == nullptr)
				{
					result << "filter not found";
				}

				// filter found
				else
				{
					int filtID = delFilter(filter);
					logger.debug("filter %d removed", filtID);

					delRelationByFilter(filtID);
					clrHost();

					result << "filter " << filtID << " removed";
				}

			}
		}

		// host without filter
		else
		{
			// without filter
			if (filter.empty() == true)
			{
				int hostID = delHost(ip, port);
				logger.debug("host %d removed", hostID);

				result << "host " << hostID << " removed";
			}

			// with filter
			else
			{
				result << "host without filter subscribed, filter ignored";
			}
		}
	}

	logger.info("%s", result.str().c_str());
	return (true);
}

void DataHandler::enqueue(const EbusSequence& eSeq)
{
	if (m_host.empty() == false)
	{
		m_ebusDataQueue.enqueue(new EbusSequence(eSeq));
		notify();
	}
}

const string DataHandler::toString()
{
	ostringstream ostr;

	ostr << endl << toStringHost();

	if (m_filter.empty() == false) ostr << endl << toStringFilter();

	if (m_relation.empty() == false) ostr << endl << toStringRelation();

	ostr << endl;

	return (ostr.str());
}

const string DataHandler::toStringHost()
{
	ostringstream ostr;

	for (Host* host : m_host)
		ostr << host->toString() << endl;

	return (ostr.str());
}

const string DataHandler::toStringFilter()
{
	ostringstream ostr;

	for (Filter* filter : m_filter)
		ostr << filter->toString() << endl;

	return (ostr.str());
}

const string DataHandler::toStringRelation()
{
	ostringstream ostr;

	for (Relation* relation : m_relation)
		ostr << relation->toString() << endl;

	return (ostr.str());
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
			send(eSeq);
			delete eSeq;
		}
	}
}

void DataHandler::send(EbusSequence* eSeq) const
{
	Logger logger = Logger("DataHandler::send");

	for (const auto& host : m_host)
		if (host->hasFilter() == false)
		{
			logger.info("to: %s:%d", host->getIP().c_str(), host->getPort());
			host->send(eSeq->toString());
		}

	for (const auto& filter : m_filter)
		if (filter->match(eSeq->getMaster()) == true)
		{
			logger.debug("%s match in %s", filter->getFilter().toString().c_str(),
				eSeq->getMaster().toString().c_str());

			for (const auto& relation : m_relation)
				if (relation->getFilterID() == filter->getID())
				{
					for (const auto& host : m_host)
						if (host->hasFilter() == true && host->getID() == relation->getHostID())
						{
							logger.info("to %s:%d", host->getIP().c_str(),
								host->getPort());
							host->send(eSeq->toString());
						}
				}
		}
}

Host* DataHandler::getHost(const string& ip, long port) const
{
	for (size_t index = 0; index < m_host.size(); index++)
		if (m_host[index]->equal(ip, port) == true) return (m_host[index]);

	return (nullptr);
}

Host* DataHandler::addHost(const string& ip, long port, bool filter)
{
	size_t index;

	for (index = 0; index < m_host.size(); index++)
		if (m_host[index]->equal(ip, port) == true) break;

	if (index == m_host.size())
		m_host.push_back(new Host(ip, port, filter));
	else
		m_host[index]->setFilter(filter);

	return (m_host[index]);
}

int DataHandler::delHost(const string& ip, long port)
{
	for (size_t index = 0; index < m_host.size(); index++)
		if (m_host[index]->equal(ip, port) == true)
		{
			Host* host = m_host[index];
			int id = host->getID();

			m_host.erase(m_host.begin() + index);
			m_host.shrink_to_fit();

			delete host;
			return (id);
		}

	return (0);
}

void DataHandler::clrHost()
{
	auto it = m_host.begin();

	while (it != m_host.end())
	{
		bool notFound = true;

		for (Relation* relation : m_relation)
			if ((*it)->getID() == relation->getHostID())
			{
				notFound = false;
				break;
			}

		if (notFound == true)
		{
			Host* host = *it;
			it = m_host.erase(it);
			delete host;
		}
		else
		{
			it++;
		}
	}

	m_host.shrink_to_fit();
}

const Filter* DataHandler::getFilter(const string& filter) const
{
	Sequence seq(filter);

	for (size_t index = 0; index < m_filter.size(); index++)
		if (m_filter[index]->equal(seq) == true) return (m_filter[index]);

	return (nullptr);
}

const Filter* DataHandler::addFilter(const string& filter)
{
	Sequence seq(filter);
	size_t index;

	for (index = 0; index < m_filter.size(); index++)
		if (m_filter[index]->equal(seq) == true) break;

	if (index == m_filter.size()) m_filter.push_back(new Filter(seq));

	return (m_filter[index]);
}

int DataHandler::delFilter(const string& filter)
{
	Sequence seq(filter);

	for (size_t index = 0; index < m_filter.size(); index++)
		if (m_filter[index]->equal(seq) == true)
		{
			Filter* _filter = m_filter[index];
			int id = _filter->getID();

			m_filter.erase(m_filter.begin() + index);
			m_filter.shrink_to_fit();

			delete _filter;
			return (id);
		}

	return (0);
}

void DataHandler::clrFilter()
{
	auto it = m_filter.begin();

	while (it != m_filter.end())
	{
		bool notFound = true;

		for (Relation* relation : m_relation)
			if ((*it)->getID() == relation->getFilterID())
			{
				notFound = false;
				break;
			}

		if (notFound == true)
		{
			Filter* filter = *it;
			it = m_filter.erase(it);
			delete filter;
		}
		else
		{
			it++;
		}
	}

	m_filter.shrink_to_fit();
}

const Relation* DataHandler::getRelation(const int hostID, const int filterID) const
{
	for (size_t index = 0; index < m_relation.size(); index++)
		if (m_relation[index]->equal(hostID, filterID) == true) return (m_relation[index]);

	return (nullptr);
}

const Relation* DataHandler::addRelation(const int hostID, const int filterID)
{
	size_t index;

	for (index = 0; index < m_relation.size(); index++)
		if (m_relation[index]->equal(hostID, filterID) == true) break;

	if (index == m_relation.size()) m_relation.push_back(new Relation(hostID, filterID));

	return (m_relation[index]);
}

void DataHandler::delRelationByHost(const int hostID)
{
	auto it = m_relation.begin();

	while (it != m_relation.end())
	{
		if ((*it)->getHostID() == hostID)
		{
			Relation* relation = *it;
			it = m_relation.erase(it);
			delete relation;
		}
		else
		{
			it++;
		}
	}

	m_relation.shrink_to_fit();
}

void DataHandler::delRelationByFilter(const int filterID)
{
	auto it = m_relation.begin();

	while (it != m_relation.end())
	{
		if ((*it)->getFilterID() == filterID)
		{
			Relation* relation = *it;
			it = m_relation.erase(it);
			delete relation;
		}
		else
		{
			it++;
		}
	}

	m_relation.shrink_to_fit();
}

