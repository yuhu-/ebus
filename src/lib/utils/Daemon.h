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

#ifndef LIBUTILS_DAEMON_H
#define LIBUTILS_DAEMON_H

#include <cstddef>
#include <cstdio>
#include <string>

using std::string;

namespace libutils
{

class Daemon
{

public:
	static Daemon& getDaemon();

	void start(const string& pidfile);

	void stop();

	bool status() const;

private:
	Daemon();
	Daemon(const Daemon&);
	Daemon& operator=(const Daemon&);

	bool m_status = false;

	string m_pidfile;
	FILE* m_pidfd = nullptr;

};

} // namespace libutils

#endif // LIBUTILS_DAEMON_H
