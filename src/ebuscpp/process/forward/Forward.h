/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebuscpp.
 *
 * ebuscpp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebuscpp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebuscpp. If not, see http://www.gnu.org/licenses/.
 */

#ifndef PROCESS_FORWARD_FORWARD_H
#define PROCESS_FORWARD_FORWARD_H

#include "Filter.h"
#include "Host.h"
#include "Relation.h"
#include "NQueue.h"
#include "Notify.h"
#include "EbusSequence.h"

#include <thread>

using libebus::EbusSequence;
using libutils::Notify;
using libutils::NQueue;
using std::thread;
using std::ostringstream;
using std::shared_ptr;

class Forward : public Notify
{

public:
	Forward();
	~Forward();

	void start();
	void stop();

	void append(const string& ip, long port, const string& filter, ostringstream& result);
	void remove(const string& ip, long port, const string& filter, ostringstream& result);

	void enqueue(const EbusSequence& eSeq);

	bool isActive() const;

	const string toString();
	const string toStringHost();
	const string toStringFilter();
	const string toStringRelation();

private:
	thread m_thread;

	bool m_running = true;

	NQueue<EbusSequence*> m_ebusDataQueue;

	vector<shared_ptr<Host>> m_host;

	vector<shared_ptr<Filter>> m_filter;

	vector<shared_ptr<Relation>> m_relation;

	void run();

	void send(EbusSequence* eSeq) const;

	const shared_ptr<Host> getHost(const string& ip, long port) const;
	const shared_ptr<Host> addHost(const string& ip, long port, bool filter);
	int delHost(const string& ip, long port);
	void clrHost();

	const shared_ptr<Filter> getFilter(const string& filter) const;
	const shared_ptr<Filter> addFilter(const string& filter);
	int delFilter(const string& filter);
	void clrFilter();

	const shared_ptr<Relation> getRelation(const int hostID, const int filterID) const;
	const shared_ptr<Relation> addRelation(const int hostID, const int filterID);
	void delRelationByHost(const int hostID);
	void delRelationByFilter(const int filterID);
};

#endif // PROCESS_FORWARD_FORWARD_H
