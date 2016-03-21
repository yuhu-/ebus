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

#ifndef EBUS_DATAHANDLER_H
#define EBUS_DATAHANDLER_H

#include "EbusSequence.h"
#include "Host.h"
#include "Filter.h"
#include "Relation.h"
#include "NQueue.h"
#include "Notify.h"

#include <thread>

using std::thread;
using std::ostringstream;

class DataHandler : public Notify
{

public:
	DataHandler();
	~DataHandler();

	void start();
	void stop();

	bool subscribe(const string& ip, long port, const string& filter, ostringstream& result);
	bool unsubscribe(const string& ip, long port, const string& filter, ostringstream& result);

	void enqueue(const EbusSequence& eSeq);

	const string toString();
	const string toStringHost();
	const string toStringFilter();
	const string toStringRelation();

private:
	thread m_thread;

	bool m_running = true;

	NQueue<EbusSequence*> m_ebusDataQueue;

	vector<Host*> m_host;

	vector<Filter*> m_filter;

	vector<Relation*> m_relation;

	void run();

	Host* getHost(const string& ip, long port) const;
	Host* addHost(const string& ip, long port, bool filter);
	int delHost(const string& ip, long port);
	void clrHost();

	const Filter* getFilter(const string& filter) const;
	const Filter* addFilter(const string& filter);
	int delFilter(const string& filter);
	void clrFilter();

	const Relation* getRelation(int hostID, int filterID) const;
	const Relation* addRelation(int hostID, int filterID);
	void delRelationByHost(int hostID);
	void delRelationByFilter(int filterID);
};

#endif // EBUS_DATAHANDLER_H
