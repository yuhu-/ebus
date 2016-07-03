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

#ifndef BASELOOP_H
#define BASELOOP_H

#include "Ebus.h"
#include "ProcessHandler.h"
#include "Network.h"

#include <cstring>

using std::ostringstream;

enum Command
{
	c_invalid, c_open, c_close, c_send, c_forward, c_dump, c_log, c_raw, c_help
};

class BaseLoop
{

public:
	BaseLoop();
	~BaseLoop();

	void run();

private:
	BaseLoop(const BaseLoop&);
	BaseLoop& operator=(const BaseLoop&);

	bool m_running = true;

	unsigned char m_ownAddress = 0;
	Ebus* m_ebus = nullptr;
	ProcessHandler* m_processHandler = nullptr;
	Network* m_network = nullptr;

	static Command findCommand(const string& item);

	string decodeMessage(const string& data);

	static bool isHex(const string& str, ostringstream& result, int nibbles);

	static bool isNum(const string& str, ostringstream& result);

	void handleForward(const vector<string>& args, ostringstream& result);

	static const string formatHelp();

};

#endif // BASELOOP_H
