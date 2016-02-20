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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Logger.h"
#include "Daemon.h"
#include "Option.h"
#include "BaseLoop.h"

#include <csignal>
#include <iostream>

#include <dirent.h>

Daemon& D = Daemon::getInstance();

BaseLoop* baseloop = nullptr;

void define_args()
{
	Option& O = Option::getOption();

	O.setVersion("" PACKAGE_STRING"");

	O.addText("Options:\n");

	O.addOption("address", "a", OptVal(0xff), dt_hex, ot_mandatory, "\tebus device address [FF]");

	O.addOption("foreground", "f", OptVal(false), dt_bool, ot_none, "run in foreground\n");

	O.addOption("device", "d", OptVal("/dev/ttyUSB0"), dt_string, ot_mandatory,
		"\tebus device (serial or network) [/dev/ttyUSB0]");

	O.addOption("nodevicecheck", "n", OptVal(false), dt_bool, ot_none, "disable test of local ebus device");

	O.addOption("reopentime", "", OptVal(60), dt_long, ot_mandatory,
		"max. time to open ebus device in 'sec' [60]\n");

	O.addOption("arbitrationtime", "", OptVal(4400), dt_long, ot_mandatory,
		"waiting time for arbitration test 'us' [4400]");

	O.addOption("receivetimeout", "", OptVal(4700), dt_long, ot_mandatory,
		"max. time for receiving of one sequence sign 'us' [4700]");

	O.addOption("lockcounter", "", OptVal(5), dt_int, ot_mandatory,
		"number of characters after a successful ebus access [5] (max: 25)");

	O.addOption("lockretries", "", OptVal(2), dt_int, ot_mandatory, "number of retries to lock ebus [2]\n");

	O.addOption("active", "", OptVal(false), dt_bool, ot_none, "\thandle broadcast and at me addressed messages\n");

	O.addOption("store", "", OptVal(false), dt_bool, ot_none, "\tstore received messages\n");

	O.addOption("port", "p", OptVal(8888), dt_int, ot_mandatory, "\tlisten port [8888]");

	O.addOption("local", "", OptVal(false), dt_bool, ot_none, "\tlisten only on localhost\n");

	O.addOption("logfile", "", OptVal("/var/log/ebusgate.log"), dt_string, ot_mandatory,
		"\tlog file name [/var/log/ebusgate.log]");

	O.addOption("loglevel", "", OptVal("info"), dt_string, ot_mandatory,
		"\tset logging level - off|error|warn|info|debug|trace [info]");

	O.addOption("raw", "", OptVal(false), dt_bool, ot_none, "\ttoggle raw output\n");

	O.addOption("pidfile", "", OptVal("/var/run/ebusgate.pid"), dt_string, ot_mandatory,
		"\tpid file name [/var/run/ebusgate.pid]\n");

	O.addOption("dump", "", OptVal(false), dt_bool, ot_none, "\ttoggle raw dump");

	O.addOption("dumpfile", "", OptVal("/tmp/ebus_dump.bin"), dt_string, ot_mandatory,
		"\tdump file name [/tmp/ebus_dump.bin]");

	O.addOption("dumpsize", "", OptVal(100), dt_long, ot_mandatory, "\tmax size for dump file in 'kB' [100]");
}

void shutdown()
{
	Logger& L = Logger::getLogger("shutdown");

	// stop threads
	if (baseloop != nullptr)
	{
		delete baseloop;
		baseloop = nullptr;
	}

	// reset all signal handlers to default
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	// delete daemon pid file
	if (D.status() == true) D.stop();

	// stop logger
	L.log(info, "ebusgate stopped");
	L.stop();

	exit(EXIT_SUCCESS);
}

void signal_handler(int sig)
{
	Logger& L = Logger::getLogger("signal_handler");

	switch (sig)
	{
	case SIGHUP:
		L.log(info, "SIGHUP received");
		break;
	case SIGINT:
		L.log(info, "SIGINT received");
		shutdown();
		break;
	case SIGTERM:
		L.log(info, "SIGTERM received");
		shutdown();
		break;
	default:
		L.log(info, "undefined signal %s", strsignal(sig));
		break;
	}
}

int main(int argc, char* argv[])
{
	Logger& L = Logger::getLogger("main");
	Option& O = Option::getOption();

	// define arguments and application variables
	define_args();

	// parse arguments
	if (O.parseArgs(argc, argv) == false) return (EXIT_SUCCESS);

	if (O.getOptVal<bool>("foreground") == true)
	{
		L.addConsole();
	}
	else
	{
		// make me daemon
		D.start(O.getOptVal<const char*>("pidfile"));
		L.addFile(O.getOptVal<const char*>("logfile"));
	}

	// trap signals that we expect to receive
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// start logger
	L.setLevel(O.getOptVal<const char*>("loglevel"));
	L.start();

	L.log(info, "ebusgate started");

	// create baseloop
	baseloop = new BaseLoop();
	baseloop->start();

	// shutdown
	shutdown();
}

