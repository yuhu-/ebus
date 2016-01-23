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

#ifndef NETWORK_NETMESSAGE_H
#define NETWORK_NETMESSAGE_H

#include "Notify.h"

#include <string>

using std::string;

class NetMessage : public Notify
{

public:
	explicit NetMessage(const string data);

	string getData() const;
	string getResult() const;
	void setResult(const string result);

private:
	string m_data;
	string m_result;

};

#endif // NETWORK_NETMESSAGE_H

