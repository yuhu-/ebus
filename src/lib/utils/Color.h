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

#ifndef LIBUTILS_COLOR_H
#define LIBUTILS_COLOR_H

#include <iostream>

#include <unistd.h>

using std::ostream;
using std::cout;
using std::cerr;
using std::clog;

namespace color
{

inline bool istty(const ostream& ostr)
{
	FILE* file = nullptr;

	if (&ostr == &cout)
		file = stdout;
	else if (&ostr == &cerr || &ostr == &clog)
		file = stderr;

	if (isatty(fileno(file))) return (true);
	return (false);
}

inline ostream& reset(ostream& ostr)
{
	if (ostr) ostr << ("\033[0m");

	return (ostr);
}

inline ostream& black(ostream& ostr)
{
	if (ostr) ostr << ("\033[1;30m");

	return (ostr);
}

inline ostream& red(ostream& ostr)
{
	if (ostr) ostr << ("\033[1;31m");

	return (ostr);
}

inline ostream& green(ostream& ostr)
{
	if (ostr) ostr << ("\033[1;32m");

	return (ostr);
}

inline ostream& yellow(ostream& ostr)
{
	if (ostr) ostr << ("\033[1;33m");

	return (ostr);
}

inline ostream& blue(ostream& ostr)
{
	if (ostr) ostr << ("\033[34m");

	return (ostr);
}

inline ostream& magenta(ostream& ostr)
{
	if (ostr) ostr << ("\033[35m");

	return (ostr);
}

inline ostream& cyan(ostream& ostr)
{
	if (ostr) ostr << ("\033[36m");

	return (ostr);
}

inline ostream& white(ostream& ostr)
{
	if (ostr) ostr << ("\033[37m");

	return (ostr);
}

} // namespace color

#endif // LIBUTILS_COLOR_H
