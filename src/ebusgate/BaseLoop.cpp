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

#include "BaseLoop.h"
#include "Option.h"
#include <Logger.h>

#include <iomanip>
#include <iterator>

using std::istringstream;
using std::istream_iterator;
using std::endl;

map<Command, string> CommandNames =
{
{ c_open, "OPEN" },
{ c_close, "CLOSE" },
{ c_send, "SEND" },
{ c_grab, "GRAB" },
{ c_active, "ACTIVE" },
{ c_store, "STORE" },
{ c_loglevel, "LOGLEVEL" },
{ c_raw, "RAW" },
{ c_dump, "DUMP" },
{ c_help, "HELP" } };

BaseLoop::BaseLoop()
{
	Option& O = Option::getOption();

	m_ownAddress = O.getInt("address") & 0xff;

	m_ebushandler = new EbusHandler(O.getInt("address") & 0xff, O.getString("device"), O.getBool("nodevicecheck"),
		O.getLong("reopentime"), O.getLong("arbitrationtime"), O.getLong("receivetimeout"),
		O.getInt("lockcounter"), O.getInt("lockretries"), O.getBool("active"), O.getBool("store"),
		O.getBool("raw"), O.getBool("dump"), O.getString("dumpfile"), O.getLong("dumpsize"));

	m_ebushandler->start();

	m_network = new Network(O.getBool("local"), O.getInt("port"), &m_netMsgQueue);
	m_network->start();
}

BaseLoop::~BaseLoop()
{
	if (m_network != nullptr)
	{
		m_network->stop();
		delete m_network;
		m_network = nullptr;
	}

	if (m_ebushandler != nullptr)
	{
		m_ebushandler->stop();
		delete m_ebushandler;
		m_ebushandler = nullptr;
	}

	while (m_netMsgQueue.size() > 0)
		delete m_netMsgQueue.dequeue();

}

void BaseLoop::start()
{
	Logger L = Logger("BaseLoop::start");

	while (true)
	{
		string result;

		// recv new message from client
		NetMessage* message = m_netMsgQueue.dequeue();
		string data = message->getData();

		string::size_type pos = 0;
		while ((pos = data.find("\r\n", pos)) != string::npos)
			data.erase(pos, 2);

		L.log(info, ">>> %s", data.c_str());

		// decode message
		if (strcasecmp(data.c_str(), "STOP") != 0)
		{
			result = decodeMessage(data);
		}
		else
		{
			result = "done";
		}

		L.log(info, "<<< %s", result.c_str());
		result += "\n\n";

		// send result to client
		message->setResult(result);
		message->notify();

		// stop daemon
		if (strcasecmp(data.c_str(), "STOP") == 0) break;
	}
}

void BaseLoop::addMessage(NetMessage* message)
{
	m_netMsgQueue.enqueue(message);
}

Command BaseLoop::getCase(const string& command)
{
	for (const auto& cmd : CommandNames)
		if (strcasecmp(cmd.second.c_str(), command.c_str()) == 0) return (cmd.first);

	return (c_invalid);
}

string BaseLoop::decodeMessage(const string& data)
{
	Logger L = Logger("BaseLoop::decodeMessage");

	ostringstream result;

	// prepare data
	istringstream istr(data);
	vector<string> args = vector<string>(istream_iterator<string>(istr), istream_iterator<string>());

	if (args.size() == 0) return ("command missing");

	size_t argPos = 1;

	switch (getCase(args[0]))
	{
	case c_invalid:
	{
		result << "command not found";
		break;
	}
	case c_open:
	{
		if (args.size() != argPos)
		{
			result << "usage: 'open'";
			break;
		}

		m_ebushandler->open();
		result << "done";
		break;
	}
	case c_close:
	{
		if (args.size() != argPos)
		{
			result << "usage: 'close'";
			break;
		}

		m_ebushandler->close();
		result << "done";
		break;
	}
	case c_send:
	{
		if (args.size() < argPos + 1)
		{
			result << "usage: 'send ZZPBSBNNDx'";
			break;
		}

		ostringstream msg;
		while (argPos < args.size())
			msg << args[argPos++];

		if (isHex(msg.str(), result) == false)
		{
			msg.str("");
			break;
		}

		EbusSequence eSeq;
		eSeq.createMaster(m_ownAddress, msg.str());

		// send message
		if (eSeq.isValid() == true)
		{
			L.log(debug, "enqueue: %s", eSeq.toStringMaster().c_str());
			EbusMessage* ebusMessage = new EbusMessage(eSeq);
			m_ebushandler->addMessage(ebusMessage);
			ebusMessage->waitNotify();
			result << ebusMessage->getResult();
			delete ebusMessage;
		}
		else
		{
			result << eSeq.toStringMaster();
			L.log(debug, "error: %s", eSeq.toStringMaster().c_str());
		}
		break;
	}
	case c_grab:
	{
		if (args.size() < argPos + 1)
		{
			result << "usage: 'grab QQZZPBSBNNDx'";
			break;
		}

		ostringstream msg;
		while (argPos < args.size())
			msg << args[argPos++];

		if (isHex(msg.str(), result) == false)
		{
			msg.str("");
			break;
		}

		// grab message
		L.log(debug, "grab: %s", msg.str().c_str());
		result << m_ebushandler->grabMessage(msg.str());
		break;
	}
	case c_active:
	{
		if (args.size() != argPos)
		{
			result << "usage: 'active'";
			break;
		}

		bool enabled = !m_ebushandler->getActive();
		m_ebushandler->setActive(enabled);
		result << (enabled ? "active mode enabled" : "active mode disabled");
		break;
	}
	case c_store:
	{
		if (args.size() != argPos)
		{
			result << "usage: 'store'";
			break;
		}

		bool enabled = !m_ebushandler->getStore();
		m_ebushandler->setStore(enabled);
		result << (enabled ? "storing enabled" : "storing disabled");
		break;
	}
	case c_loglevel:
	{
		if (args.size() != argPos + 1)
		{
			result << "usage: 'loglevel level' (level: off|error|warn|info|debug|trace)";
			break;
		}

		L.setLevel(calcLevel(args[argPos]));
		result << "done";
		break;

	}
	case c_raw:
	{
		if (args.size() != argPos)
		{
			result << "usage: 'raw'";
			break;
		}

		bool enabled = !m_ebushandler->getLogRaw();
		m_ebushandler->setLogRaw(enabled);
		result << (enabled ? "raw output enabled" : "raw output disabled");
		break;
	}
	case c_dump:
	{
		if (args.size() != argPos)
		{
			result << "usage: 'dump'";
			break;
		}

		bool enabled = !m_ebushandler->getDumpRaw();
		m_ebushandler->setDumpRaw(enabled);
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

bool BaseLoop::isHex(const string& str, ostringstream& result)
{
	if ((str.length() % 2) != 0)
	{
		result << "invalid hex string";
		return (false);
	}

	for (size_t i = 0; i < str.size(); ++i)
	{
		if (isxdigit(str[i]) == false)
		{
			result << "invalid char " << str[i];
			return (false);
		}
	}

	return (true);
}

const string BaseLoop::formatHelp()
{
	ostringstream ostr;
	ostr << "commands:" << endl;
	ostr << " open       - connect with ebus           'open'" << endl;
	ostr << " close      - disconnect from ebus        'close'" << endl;
	ostr << " send       - write ebus values           'send ZZPBSBNNDx'" << endl;
	ostr << " grab       - grab ebus values from store 'grab QQZZPBSBNNDx'" << endl;
	ostr << " active     - toggle active mode          'active'" << endl;
	ostr << " store      - toggle storing data         'store'" << endl;
	ostr << " loglevel   - change logging level        'loglevel level'" << endl;
	ostr << " raw        - toggle raw output           'raw'" << endl;
	ostr << " dump       - toggle raw dump             'dump'" << endl;
	ostr << " stop       - stop daemon                 'stop'" << endl;
	ostr << " quit       - close connection            'quit'" << endl;
	ostr << " help       - print this page             'help'";

	return (ostr.str());
}
