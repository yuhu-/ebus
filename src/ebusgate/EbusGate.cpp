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

#include "Daemon.h"
#include "Options.h"
#include "BaseLoop.h"
#include "Logger.h"

#include <csignal>
#include <iostream>

#include <dirent.h>

BaseLoop* baseloop = nullptr;

void define_args()
{
	Options& O = Options::getOption();

	O.setVersion("" PACKAGE_STRING"");

//	O.addText("Options:\n");

	O.addHex("address", "a", 0xff, "\tebus device address [FF]");

	O.addBool("foreground", "f", false, "run in foreground\n");

	O.addString("device", "d", "/dev/ttyUSB0", "\tebus device (serial or network) [/dev/ttyUSB0]");

	O.addBool("nodevicecheck", "n", false, "disable test of local ebus device");

	O.addLong("reopentime", "", 60, "max. time to open ebus device in 'sec' [60]\n");

	O.addLong("arbitrationtime", "", 4400, "waiting time for arbitration test 'us' [4400]");

	O.addLong("receivetimeout", "", 4700, "max. time for receiving of one sequence sign 'us' [4700]");

	O.addInt("lockcounter", "", 5, "number of characters after a successful ebus access [5] (max: 25)");

	O.addInt("lockretries", "", 2, "number of retries to lock ebus [2]\n");

	O.addBool("active", "", false, "\thandle broadcast and at me addressed messages\n");

	O.addBool("store", "", false, "\tstore received messages\n");

	O.addInt("port", "p", 8888, "\tlisten port [8888]");

	O.addBool("local", "", false, "\tlisten only on localhost\n");

	O.addString("logfile", "", "/var/log/ebusgate.log", "\tlog file name [/var/log/ebusgate.log]");

	O.addString("loglevel", "", "info", "\tset logging level - off|error|warn|info|debug|trace [info]");

	O.addBool("raw", "", false, "\ttoggle raw output\n");

	O.addString("pidfile", "", "/var/run/ebusgate.pid", "\tpid file name [/var/run/ebusgate.pid]\n");

	O.addBool("dump", "", false, "\ttoggle raw dump");

	O.addString("dumpfile", "", "/tmp/ebus_dump.bin", "\tdump file name [/tmp/ebus_dump.bin]");

	O.addLong("dumpsize", "", 100, "\tmax size for dump file in 'kB' [100]");
}

void shutdown()
{
	Logger logger = Logger("shutdown");

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
	if (Daemon::getDaemon().status() == true) Daemon::getDaemon().stop();

	// stop logger
	logger.info("ebusgate stopped");

	logger.stop();
	exit(EXIT_SUCCESS);

}

void signal_handler(int sig)
{
	Logger logger = Logger("signal_handler");

	switch (sig)
	{
	case SIGHUP:
		logger.info("SIGHUP received");
		break;
	case SIGINT:
		logger.info("SIGINT received");
		shutdown();
		break;
	case SIGTERM:
		logger.info("SIGTERM received");
		shutdown();
		break;
	default:
		logger.info("undefined signal %s", strsignal(sig));
		break;
	}
}

int main(int argc, char* argv[])
{
	// define arguments and application variables
	define_args();

	// parse arguments
	Options& O = Options::getOption();
	if (O.parse(argc, argv) == false) return (EXIT_SUCCESS);

	Logger logger = Logger("main");

	if (O.getBool("foreground") == true)
	{
		logger.addConsole();
	}
	else
	{
		// make me daemon
		Daemon::getDaemon().start(O.getString("pidfile"));
		logger.addFile(O.getString("logfile"));
	}

	// trap signals that we expect to receive
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// start logger
	logger.setLevel(O.getString("loglevel"));
	logger.start();

	logger.info("ebusgate started");

	// create baseloop
	baseloop = new BaseLoop();
	baseloop->start();

	// shutdown and exit
	shutdown();

}

