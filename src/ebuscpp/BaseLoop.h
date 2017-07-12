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

#ifndef BASELOOP_H
#define BASELOOP_H

#include "EbusLogger.h"
#include "EbusFSM.h"
#include "Forward.h"
#include "Network.h"

#include <cstring>

using libebus::EbusFSM;
using libebus::Action;
using std::ostringstream;
using std::make_shared;

class BaseLoop
{

	enum class Command
	{
		invalid, open, close, send, forward, dump, log, help
	};

	map<Command, string> CommandNames =
	{
	{ Command::open, "OPEN" },
	{ Command::close, "CLOSE" },
	{ Command::send, "SEND" },
	{ Command::forward, "FORWARD" },
	{ Command::log, "LOG" },
	{ Command::dump, "DUMP" },
	{ Command::help, "HELP" } };

public:
	BaseLoop();
	~BaseLoop();

	void run();

private:
	BaseLoop(const BaseLoop&);
	BaseLoop& operator=(const BaseLoop&);

	bool m_running = true;

	unsigned char m_address = 0;
	unique_ptr<Forward> m_forward = nullptr;
	shared_ptr<EbusLogger> m_logger = make_shared<EbusLogger>();
	unique_ptr<EbusFSM> m_ebusFSM = nullptr;
	unique_ptr<Network> m_network = nullptr;

	Command findCommand(const string& item);

	string decodeMessage(const string& data);

	static bool isNum(const string& str, ostringstream& result);

	void handleForward(const vector<string>& args, ostringstream& result);

	static const string formatHelp();

	Action identifyAction(EbusSequence& eSeq);
	void publishMessage(EbusSequence& eSeq);

};

#endif // BASELOOP_H
