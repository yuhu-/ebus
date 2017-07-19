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

#include "BaseLoop.h"
#include "Options.h"
#include "Logger.h"
#include "EbusCommon.h"
#include "Message.h"

#include <iomanip>
#include <iterator>

#include <netdb.h>
#include <arpa/inet.h>

using libutils::Options;
using libebus::isHex;
using libebus::Message;
using libebus::EbusSequence;
using libebus::Reaction;
using std::istringstream;
using std::istream_iterator;
using std::endl;
using std::to_string;
using std::make_unique;
using std::bind;

BaseLoop::BaseLoop()
{
	Options& options = Options::getOption();

	m_address = options.getInt("address") & 0xff;

	m_forward = make_unique<Forward>();

	m_ebusFSM = make_unique<EbusFSM>(m_address, options.getString("device"), options.getBool("devicecheck"), m_logger,
		bind(&BaseLoop::identify, this, std::placeholders::_1), bind(&BaseLoop::publish, this, std::placeholders::_1));

	m_ebusFSM->setReopenTime(options.getLong("reopentime"));
	m_ebusFSM->setArbitrationTime(options.getLong("arbitrationtime"));
	m_ebusFSM->setReceiveTimeout(options.getLong("receivetimeout"));
	m_ebusFSM->setLockCounter(options.getInt("lockcounter"));
	m_ebusFSM->setLockRetries(options.getInt("lockretries"));
	m_ebusFSM->setDump(options.getBool("dump"));
	m_ebusFSM->setDumpFile(options.getString("dumpfile"));
	m_ebusFSM->setDumpFileMaxSize(options.getLong("dumpsize"));

	m_network = make_unique<Network>(options.getBool("local"), options.getInt("port"));
}

void BaseLoop::run()
{
	while (true)
	{
		string result;

		// recv new message from client
		NetMessage* message = m_network->dequeue();
		string data = message->getData();

		string::size_type pos = 0;
		while ((pos = data.find("\r\n", pos)) != string::npos)
			data.erase(pos, 2);

		LIBLOGGER_INFO(">>> %s", data.c_str());

		// decode message
		if (strcasecmp(data.c_str(), "STOP") != 0)
			result = decodeMessage(data);
		else
			result = "stopped";

		LIBLOGGER_INFO("<<< %s", result.c_str());
		result += "\n\n";

		// send result to client
		message->setResult(result);
		message->notify();

		// stop daemon
		if (strcasecmp(data.c_str(), "STOP") == 0) break;
	}
}

BaseLoop::Command BaseLoop::findCommand(const string& command)
{
	for (const auto& cmd : CommandNames)
		if (strcasecmp(cmd.second.c_str(), command.c_str()) == 0) return (cmd.first);

	return (Command::invalid);
}

string BaseLoop::decodeMessage(const string& data)
{
	ostringstream result;

	// prepare data
	istringstream istr(data);
	vector<string> args = vector<string>(istream_iterator<string>(istr), istream_iterator<string>());

	if (args.size() == 0) return ("command missing");

	switch (findCommand(args[0]))
	{
	case Command::invalid:
	{
		result << "command not found";
		break;
	}
	case Command::open:
	{
		if (args.size() != 1)
		{
			result << "usage: 'open'";
			break;
		}

		m_ebusFSM->open();
		result << "connected";
		break;
	}
	case Command::close:
	{
		if (args.size() != 1)
		{
			result << "usage: 'close'";
			break;
		}

		m_ebusFSM->close();
		result << "disconnected";
		break;
	}
	case Command::send:
	{
		if (args.size() != 2)
		{
			result << "usage: 'send ZZPBSBNNDx'";
			break;
		}

		if (isHex(args[1], result, 2) == true)
		{
			EbusSequence eSeq;
			eSeq.createMaster(m_address, args[1]);
			int state = m_ebusFSM->transmit(eSeq);

			if (state == SEQ_OK)
				result << eSeq.toString();
			else
				result << EbusFSM::errorText(state);

		}

		break;
	}
	case Command::forward:
	{
		if (args.size() < 2 || args.size() > 4)
		{
			result << "usage: 'forward [-d] server:port [filter]'";
			break;
		}

		handleForward(args, result);

		break;
	}
	case Command::log:
	{
		if (args.size() != 2)
		{
			result << "usage: 'log level' (level: off|error|warn|info|debug|trace)";
			break;
		}

		LIBLOGGER_LEVEL(args[1]);
		result << "changed";
		break;

	}
	case Command::dump:
	{
		if (args.size() > 2)
		{
			result << "usage: 'dump [on|off]'";
			break;
		}

		if (args.size() == 1)
		{
			result << "dump is " << (m_ebusFSM->getDump() == true ? "enabled" : "disabled");
			break;
		}

		if (strcasecmp(args[1].c_str(), "ON") == 0)
		{
			if (m_ebusFSM->getDump() == false) m_ebusFSM->setDump(true);
			result << "dump enabled";
			break;
		}

		if (strcasecmp(args[1].c_str(), "OFF") == 0)
		{
			if (m_ebusFSM->getDump() == true) m_ebusFSM->setDump(false);
			result << "dump disabled";
			break;
		}

		result << "usage: 'dump [on|off]'";
		break;
	}
	case Command::help:
	{
		result << formatHelp();
		break;
	}
	}

	return (result.str());
}

bool BaseLoop::isNum(const string& str, ostringstream& result)
{
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (isdigit(str[i]) == false)
		{
			result << "invalid char '" << str[i] << "'";
			return (false);
		}
	}

	return (true);
}

void BaseLoop::handleForward(const vector<string>& args, ostringstream& result)
{
	bool remove = false;
	string dstIP = "";
	long dstPort = -1;
	string filter = "";

	size_t argPos = 1;

	while (argPos < args.size())
	{
		if (args[argPos] == "-d")
		{
			remove = true;
		}
		else if (args[argPos].find(':') != string::npos)
		{
			dstIP = args[argPos].substr(0, args[argPos].find(':'));

			struct addrinfo hints, *servinfo;
			memset(&hints, 0, sizeof hints);

			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;

			if (getaddrinfo(dstIP.c_str(), nullptr, &hints, &servinfo) < 0)
			{
				result << "server '" << dstIP << "' is invalid";
				return;
			}

			char ip[INET_ADDRSTRLEN];
			struct sockaddr_in* address = (struct sockaddr_in*) servinfo->ai_addr;

			dstIP = inet_ntop(AF_INET, (struct in_addr*) &(address->sin_addr.s_addr), ip,
			INET_ADDRSTRLEN);

			freeaddrinfo(servinfo);

			if (isNum(args[argPos].substr(args[argPos].find(':') + 1), result) == false) return;

			dstPort = strtol(args[argPos].substr(args[argPos].find(':') + 1).c_str(), nullptr, 10);

			if (dstPort < 0 || dstPort > 65535)
			{
				result << "port '" << dstPort << "' is invalid (0-65535)";
				return;
			}
		}
		else
		{
			filter = args[argPos];
			if (isHex(filter, result, 1) == false)
			{
				result << " in filter " << args[argPos];
				return;
			}
		}

		argPos++;
	}

	if (dstIP.empty() == true || dstPort == -1)
	{
		result << "server:port is missing";
		return;
	}

	if (remove == true)
		m_forward->remove(dstIP, dstPort, filter, result);
	else
		m_forward->append(dstIP, dstPort, filter, result);

}

const string BaseLoop::formatHelp()
{
	ostringstream ostr;
	ostr << "commands:" << endl;
	ostr << " open     - open ebus connection" << endl;
	ostr << " close    - close ebus connection" << endl << endl;

	ostr << " send     - write message onto ebus 'send ZZPBSBNNDx'" << endl << endl;

	ostr << " forward  - forward ebus messages 'forward [-d] server:port [filter]'" << endl;
	ostr << "               filter: ebus sequence; without filter all messages will passed" << endl;
	ostr << "               server: either ip address or hostname" << endl;
	ostr << "               port:   target udp port number" << endl << endl;

	ostr << " log      - change logging level 'log level'" << endl;

	ostr << " dump     - enable/disable raw data dumping 'dump [on|off]'" << endl << endl;

	ostr << " stop     - shutdown daemon" << endl;
	ostr << " quit     - close tcp connection" << endl << endl;

	ostr << " help     - print this page";

	return (ostr.str());
}

Reaction BaseLoop::identify(EbusSequence& eSeq)
{
	LIBLOGGER_DEBUG("identify %s", eSeq.toStringLog().c_str());

	if (eSeq.getMaster().contains("0700") == true)
	{
		return (Reaction::ignore);
	}

	if (eSeq.getMaster().contains("0704") == true)
	{
		eSeq.createSlave("0a7a50524f585901010101");
		return (Reaction::response);
	}

	if (eSeq.getMaster().contains("07fe") == true)
	{
		eSeq.clear();
		eSeq.createMaster(m_address, SEQ_BROAD, "07ff00");
		// ToDo set some flag in main loop and transmit
		// ...int reuslt = m_ebusFSM->transmit(eSeq);
		return (Reaction::ignore);
	}

	if (eSeq.getMaster().contains("b505") == true)
	{
		return (Reaction::ignore);
	}

	if (eSeq.getMaster().contains("b516") == true)
	{
		return (Reaction::ignore);
	}

	return (Reaction::undefined);
}

void BaseLoop::publish(EbusSequence& eSeq)
{
	if (m_forward->isActive())
	{
		LIBLOGGER_DEBUG("forward %s", eSeq.toStringLog().c_str());
		m_forward->enqueue(eSeq);
	}
}
