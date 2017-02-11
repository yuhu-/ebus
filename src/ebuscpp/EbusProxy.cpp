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

using libutils::Options;
using libutils::Daemon;
using std::make_unique;

unique_ptr<BaseLoop> baseloop = nullptr;

void define_args()
{
	Options& options = Options::getOption();

	options.setVersion("ebusproxy is part of " "" PACKAGE_STRING"");

	options.addDescription(" ebusproxy provides a communication interface too ebus equipped systems.");

	options.addHex("address", "a", 0xff, "ebus device address [FF]");
	options.addString("device", "d", "/dev/ttyUSB0", "ebus device (serial or network) [/dev/ttyUSB0]");
	options.addBool("devicecheck", "c", true, "sanity check of serial ebus device [yes]");
	options.addLong("reopentime", "", 60, "max. time to open ebus device in 'sec' [60]", 1);

	options.addLong("arbitrationtime", "", 4400, "waiting time for arbitration test 'us' [4400]");
	options.addLong("receivetimeout", "", 4700, "max. time for receiving of one sequence sign 'us' [4700]");
	options.addInt("lockcounter", "", 5, "number of characters after a successful ebus access (max: 25) [5]");
	options.addInt("lockretries", "", 2, "number of retries to lock ebus [2]", 1);

	options.addBool("dump", "", false, "enable/disable raw data dumping [no]");
	options.addString("dumpfile", "", "/tmp/ebus_dump.bin", "dump file name [/tmp/ebus_dump.bin]");
	options.addLong("dumpsize", "", 100, "max size for dump file in 'kB' [100]", 1);

	options.addInt("port", "p", 8888, "listen port [8888]");
	options.addBool("local", "", false, "listen only on localhost [no]", 1);

	options.addBool("foreground", "f", false, "run in foreground [no]", 1);

	options.addString("pidfile", "", "/var/run/ebusproxy.pid", "pid file name [/var/run/ebusproxy.pid]", 1);

	options.addString("logfile", "", "/var/log/ebusproxy.log", "log file name [/var/log/ebusproxy.log]");
	options.addString("loglevel", "", "info", "set logging level - off|error|warn|info|debug|trace [info]");
	options.addBool("showfunction", "", false, "show function names in logging [no]");

}

void shutdown()
{
	// stop threads
	if (baseloop != nullptr) baseloop.reset();

	// reset all signal handlers to default
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	// delete daemon pid file
	if (Daemon::getDaemon().status() == true) Daemon::getDaemon().stop();

	// stop logger
	LIBLOGGER_INFO("EbusProxy stopped");

	exit(EXIT_SUCCESS);

}

void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGHUP:
		LIBLOGGER_INFO("SIGHUP received");
		break;
	case SIGINT:
		LIBLOGGER_INFO("SIGINT received");
		shutdown();
		break;
	case SIGTERM:
		LIBLOGGER_INFO("SIGTERM received");
		shutdown();
		break;
	default:
		LIBLOGGER_INFO("undefined signal %s", strsignal(sig));
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

	if (options.getBool("foreground") == true)
	{
		LIBLOGGER_CONSOLE();
	}
	else
	{
		// make me daemon
		Daemon::getDaemon().start(options.getString("pidfile"));
		LIBLOGGER_FILE(options.getString("logfile"));
	}

	LIBLOGGER_LEVEL(options.getString("loglevel"));
	LIBLOGGER_SHOWFUNCTION(options.getBool("showfunction"));
	LIBLOGGER_FUNCTIONLENGTH(22);

	LIBLOGGER_INFO("EbusProxy started");

	// trap signals that we expect to receive
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// create baseloop
	baseloop = make_unique<BaseLoop>();
	baseloop->run();

	// shutdown and exit
	shutdown();

}

