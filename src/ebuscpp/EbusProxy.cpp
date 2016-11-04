/*
 * Copyright (C) Roland Jax 2012-2016 <roland.jax@liwest.at>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
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
	Options& options = Options::getOption();

	options.setVersion("" PACKAGE_STRING"");

	options.addDescription(" ebusproxy provides a communication interface too ebus equipped systems.");

	options.addHex("address", "a", 0xff, "\tebus device address [FF]");

	options.addString("device", "d", "/dev/ttyUSB0", "\tebus device (serial or network) [/dev/ttyUSB0]");

	options.addBool("nodevicecheck", "n", false, "disable test of local ebus device");

	options.addLong("reopentime", "", 60, "max. time to open ebus device in 'sec' [60]\n");

	options.addLong("arbitrationtime", "", 4400, "waiting time for arbitration test 'us' [4400]");

	options.addLong("receivetimeout", "", 4700, "max. time for receiving of one sequence sign 'us' [4700]");

	options.addInt("lockcounter", "", 5, "number of characters after a successful ebus access [5] (max: 25)");

	options.addInt("lockretries", "", 2, "number of retries to lock ebus [2]\n");

	options.addBool("dump", "", false, "\tenable/disable raw data dumping");

	options.addString("dumpfile", "", "/tmp/ebus_dump.bin", "\tdump file name [/tmp/ebus_dump.bin]");

	options.addLong("dumpsize", "", 100, "\tmax size for dump file in 'kB' [100]\n");

	options.addInt("port", "p", 8888, "\tlisten port [8888]");

	options.addBool("local", "", false, "\tlisten only on localhost\n");

	options.addBool("foreground", "f", false, "run in foreground\n");

	options.addString("pidfile", "", "/var/run/ebusproxy.pid", "\tpid file name [/var/run/ebusproxy.pid]\n");

	options.addString("logfile", "", "/var/log/ebusproxy.log", "\tlog file name [/var/log/ebusproxy.log]");

	options.addString("loglevel", "", "info", "\tset logging level - off|error|warn|info|debug|trace [info]");

	options.addBool("lograw", "", false, "\tenable/disable raw data logging");

}

void shutdown()
{
	Logger logger = Logger("EbusCPP::shutdown");

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
	logger.info("stopped");

	logger.stop();
	exit(EXIT_SUCCESS);

}

void signal_handler(int sig)
{
	Logger logger = Logger("EbusProxy::signal_handler");

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
	Options& options = Options::getOption();
	if (options.parse(argc, argv) == false) return (EXIT_SUCCESS);

	Logger logger = Logger("EbusProxy::main");

	if (options.getBool("foreground") == true)
	{
		logger.addConsole();
	}
	else
	{
		// make me daemon
		Daemon::getDaemon().start(options.getString("pidfile"));
		logger.addFile(options.getString("logfile"));
	}

	// trap signals that we expect to receive
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// start logger
	logger.setLevel(options.getString("loglevel"));
	logger.start();

	logger.info("started");

	// create baseloop
	baseloop = new BaseLoop();
	baseloop->run();

	// shutdown and exit
	shutdown();

}

