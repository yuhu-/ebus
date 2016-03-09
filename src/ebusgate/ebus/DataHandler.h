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
#include "NQueue.h"
#include "Notify.h"

#include <thread>
#include <map>
#include <tuple>

using std::thread;
using std::map;
using std::tuple;
using std::ostringstream;

class DataHandler : public Notify
{

	struct Host
	{
		tuple<string, long> ipPort;
		bool filter;
		int id;
	};

	struct Filter
	{
		Sequence seq;
		int id;
	};

public:
	~DataHandler();

	void start();
	void stop();

	bool subscribe(const string& ip, const long& port, const string& filter, ostringstream& result);
	bool unsubscribe(const string& ip, const long& port, const string& filter, ostringstream& result);

	const string toString();
	const string toStringHosts();
	const string toStringFilters();
	const string toStringHostFilter();

	void enqueue(const EbusSequence& eSeq);

private:
	thread m_thread;

	bool m_running = true;

	NQueue<EbusSequence*> m_ebusDataQueue;

	static int hostID;

	vector<Host> m_hosts;

	static int filterID;

	vector<Filter> m_filter;

	vector<tuple<int, int>> m_hostFilter;

	void run();
};

#endif // EBUS_DATAHANDLER_H
