/*
 * Copyright (C) Roland Jax 2012-2017 <roland.jax@liwest.at>
 *
 * This file is part of ebusfsm.
 *
 * ebusfsm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusfsm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusfsm. If not, see http://www.gnu.org/licenses/.
 */

#ifndef EBUSFSM_UTILS_COLOR_H
#define EBUSFSM_UTILS_COLOR_H

#include <iostream>

#include <unistd.h>

namespace ebusfsm
{
namespace color
{

inline bool istty(const std::ostream& ostr)
{
	std::FILE* file = nullptr;

	if (&ostr == &std::cout)
		file = stdout;
	else if (&ostr == &std::cerr || &ostr == &std::clog)
		file = stderr;

	if (isatty(fileno(file))) return (true);
	return (false);
}

inline std::ostream& reset(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[0m");

	return (ostr);
}

inline std::ostream& black(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[1;30m");

	return (ostr);
}

inline std::ostream& red(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[1;31m");

	return (ostr);
}

inline std::ostream& green(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[1;32m");

	return (ostr);
}

inline std::ostream& yellow(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[1;33m");

	return (ostr);
}

inline std::ostream& blue(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[34m");

	return (ostr);
}

inline std::ostream& magenta(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[35m");

	return (ostr);
}

inline std::ostream& cyan(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[36m");

	return (ostr);
}

inline std::ostream& white(std::ostream& ostr)
{
	if (ostr) ostr << ("\033[37m");

	return (ostr);
}

} // namespace color
} // namespace ebusfsm

#endif // EBUSFSM_UTILS_COLOR_H
