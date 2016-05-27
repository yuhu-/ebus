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

#include "Options.h"
#include "BaseLoop.h"
#include "Logger.h"

#include <iomanip>
#include <iterator>

#include <netdb.h>
#include <arpa/inet.h>

using std::istringstream;
using std::istream_iterator;
using std::endl;
using std::to_string;

map<Command, string> CommandNames =
{
{ c_open, "OPEN" },
{ c_close, "CLOSE" },
{ c_send, "SEND" },
{ c_forward, "FORWARD" },
{ c_log, "LOG" },
{ c_raw, "RAW" },
{ c_dump, "DUMP" },
{ c_help, "HELP" } };

BaseLoop::BaseLoop()
{
	Options& options = Options::getOption();

	m_ownAddress = options.getInt("address") & 0xff;

	m_ebusHandler = new EbusHandler(options.getInt("address") & 0xff, options.getString("device"),
		options.getBool("nodevicecheck"), options.getLong("reopentime"), options.getLong("arbitrationtime"),
		options.getLong("receivetimeout"), options.getInt("lockcounter"), options.getInt("lockretries"),
		options.getBool("dump"), options.getString("dumpfile"), options.getLong("dumpsize"),
		options.getBool("lograw"));

	m_networkHandler = new NetworkHandler(options.getBool("local"), options.getInt("port"));

}

BaseLoop::~BaseLoop()
{
	if (m_networkHandler != nullptr)
	{
		delete m_networkHandler;
		m_networkHandler = nullptr;
	}

	if (m_ebusHandler != nullptr)
	{
		delete m_ebusHandler;
		m_ebusHandler = nullptr;
	}
}

void BaseLoop::run()
{
	Logger logger = Logger("BaseLoop::run");

	while (true)
	{
		string result;

		// recv new message from client
		NetMessage* message = m_networkHandler->dequeue();
		string data = message->getData();

		string::size_type pos = 0;
		while ((pos = data.find("\r\n", pos)) != string::npos)
			data.erase(pos, 2);

		logger.info(">>> %s", data.c_str());

		// decode message
		if (strcasecmp(data.c_str(), "STOP") != 0)
			result = decodeMessage(data);
		else
			result = "stopped";

		logger.info("<<< %s", result.c_str());
		result += "\n\n";

		// send result to client
		message->setResult(result);
		message->notify();

		// stop daemon
		if (strcasecmp(data.c_str(), "STOP") == 0) break;
	}
}

Command BaseLoop::findCommand(const string& command)
{
	for (const auto& cmd : CommandNames)
		if (strcasecmp(cmd.second.c_str(), command.c_str()) == 0) return (cmd.first);

	return (c_invalid);
}

string BaseLoop::decodeMessage(const string& data)
{
	Logger logger = Logger("BaseLoop::decodeMessage");

	ostringstream result;

	// prepare data
	istringstream istr(data);
	vector<string> args = vector<string>(istream_iterator<string>(istr), istream_iterator<string>());

	if (args.size() == 0) return ("command missing");

	size_t argPos = 1;

	switch (findCommand(args[0]))
	{
	case c_invalid:
	{
		result << "command not found";
		break;
	}
	case c_open:
	{
		if (args.size() != 1)
		{
			result << "usage: 'open'";
			break;
		}

		m_ebusHandler->open();
		result << "connected";
		break;
	}
	case c_close:
	{
		if (args.size() != 1)
		{
			result << "usage: 'close'";
			break;
		}

		m_ebusHandler->close();
		result << "disconnected";
		break;
	}
	case c_send:
	{
		if (args.size() != 2)
		{
			result << "usage: 'send ZZPBSBNNDx'";
			break;
		}

		if (isHex(args[argPos], result, 2) == true)
		{
			EbusSequence eSeq;
			eSeq.createMaster(m_ownAddress, args[argPos]);

			// send message
			if (eSeq.getMasterState() == EBUS_OK)
			{
				logger.debug("enqueue: %s", eSeq.toStringMaster().c_str());
				EbusMessage* ebusMessage = new EbusMessage(eSeq);
				m_ebusHandler->enqueue(ebusMessage);
				ebusMessage->waitNotify();
				result << ebusMessage->getResult();
				delete ebusMessage;
				break;
			}
			else
			{
				result << eSeq.toStringMaster();
			}
		}

		logger.debug("error: %s", result.str().c_str());

		break;
	}
	case c_forward:
	{
		if (args.size() < 2 || args.size() > 4)
		{
			result << "usage: 'forward [-d] server:port [filter]'";
			break;
		}

		handleForward(args, result);

		break;
	}
	case c_log:
	{
		if (args.size() != 2)
		{
			result << "usage: 'log level' (level: off|error|warn|info|debug|trace)";
			break;
		}

		logger.setLevel(calcLevel(args[argPos]));
		result << "changed";
		break;

	}
	case c_raw:
	{
		if (args.size() > 2)
		{
			result << "usage: 'raw [on|off]'";
			break;
		}

		if (args.size() == 1)
		{
			result << "raw output is " << (m_ebusHandler->getRaw() == true ? "enabled" : "disabled");
			break;
		}

		bool enabled = !m_ebusHandler->getRaw();
		m_ebusHandler->setRaw(enabled);
		result << (enabled ? "raw output enabled" : "raw output disabled");
		break;
	}
	case c_dump:
	{
		if (args.size() > 2)
		{
			result << "usage: 'dump [on|off]'";
			break;
		}

		if (args.size() == 1)
		{
			result << "dump output is " << (m_ebusHandler->getDump() == true ? "enabled" : "disabled");
			break;
		}

		bool enabled = !m_ebusHandler->getDump();
		m_ebusHandler->setDump(enabled);
		result << (enabled ? "raw dump enabled" : "raw dump disabled");
		break;
	}
	case c_help:
	{
		result << formatHelp();
		break;
	}
	}

	return (result.str());
}

bool BaseLoop::isHex(const string& str, ostringstream& result, int nibbles)
{
	if ((str.length() % nibbles) != 0)
	{
		result << "invalid hex string";
		return (false);
	}

	for (size_t i = 0; i < str.size(); ++i)
	{
		if (isxdigit(str[i]) == false)
		{
			result << "invalid char '" << str[i] << "'";
			return (false);
		}
	}

	return (true);
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

	m_ebusHandler->forward(remove, dstIP, dstPort, filter, result);
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
	ostr << " raw      - enable/disable raw data logging 'raw [on|off]'" << endl << endl;

	ostr << " dump     - enable/disable raw data dumping 'dump [on|off]'" << endl << endl;

	ostr << " stop     - shutdown daemon" << endl;
	ostr << " quit     - close tcp connection" << endl << endl;

	ostr << " help     - print this page";

	return (ostr.str());
}
