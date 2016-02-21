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

#include "Daemon.h"

#include <iostream>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using std::cerr;
using std::endl;

Daemon& Daemon::getDaemon()
{
	static Daemon daemon;
	return (daemon);
}

void Daemon::start(const string pidfile)
{
	m_pidfile = pidfile;

	pid_t pid;

	// fork off the parent process
	pid = fork();

	if (pid < 0)
	{
		cerr << "daemon fork() failed." << endl;
		exit(EXIT_FAILURE);
	}

	// If we got a good PID, then we can exit the parent process
	if (pid > 0)
	{
		// printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}

	// At this point we are executing as the child process

	// Set file permissions 750
	umask(S_IWGRP | S_IRWXO);

	// Create a new SID for the child process and
	// detach the process from the parent (normally a shell)
	if (setsid() < 0)
	{
		cerr << "daemon setsid() failed." << endl;
		exit(EXIT_FAILURE);
	}

	// Change the current working directory. This prevents the current
	// directory from being locked; hence not being able to remove it.
	if (chdir("/tmp") < 0)
	{  //DAEMON_WORKDIR
		cerr << "daemon chdir() failed." << endl;
		exit(EXIT_FAILURE);
	}

	// Close stdin, stdout and stderr
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	// write pidfile and try to lock it
	m_pidfd = fopen(m_pidfile.c_str(), "w+");

	umask(S_IWGRP | S_IRWXO);

	if (m_pidfd != nullptr)
	{
		setbuf(m_pidfd, nullptr);
		if (lockf(fileno(m_pidfd), F_TLOCK, 0) < 0 || fprintf(m_pidfd, "%d\n", getpid()) <= 0)
		{
			fclose(m_pidfd);
			m_pidfd = nullptr;
		}
	}

	if (m_pidfd == nullptr)
	{
		cerr << "can't open pid file: " << m_pidfile << endl;
		exit(EXIT_FAILURE);
	}

	m_status = true;
}

void Daemon::stop()
{
	if (m_pidfd != nullptr)
	{
		if (fclose(m_pidfd) < 0) return;
		remove(m_pidfile.c_str());
	}
}

bool Daemon::status() const
{
	return (m_status);
}

Daemon::Daemon()
{
}
