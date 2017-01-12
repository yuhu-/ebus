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

#ifndef PROCESS_FORWARD_HOST_H
#define PROCESS_FORWARD_HOST_H

#include "Client.h"

#include <string>

using libnetwork::Client;
using libnetwork::Socket;
using std::string;

class Host
{

public:
	Host(const string& ip, const long port, const bool filter);
	~Host();

	int getID() const;

	string getIP() const;
	long getPort() const;

	void setFilter(const bool filter);
	bool hasFilter() const;

	bool equal(const string& ip, const long port) const;

	void send(const string& message);

	const string toString();

private:
	static int uniqueID;

	int m_id;
	bool m_filter;

	Client m_client;
	Socket* m_socket;
};

#endif // PROCESS_FORWARD_HOST_H

