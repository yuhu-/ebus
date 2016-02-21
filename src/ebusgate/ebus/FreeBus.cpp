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

#include "FreeBus.h"
#include "Listen.h"
#include "Logger.h"

FreeBus FreeBus::m_freeBus;

int FreeBus::run(EbusHandler* h)
{
	Logger& L = Logger::getLogger("FreeBus::run");

	unsigned char byte = SYN;

	int result = writeRead(h, byte, 0);
	if (result != DEV_OK) return (result);

	L.log(debug, "ebus freed");

	reset(h);
	h->changeState(Listen::getInstance());

	return (result);
}

FreeBus::FreeBus()
{
}

const string FreeBus::toString() const
{
	return ("FreeBus");
}
