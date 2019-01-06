/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
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

#include <EbusFSM.h>

#include <iostream>
#include <functional>

#include <unistd.h>

ebusfsm::Reaction identify(ebusfsm::EbusSequence& eSeq)
{
	std::cout << "identify: " << eSeq.toString().c_str() << std::endl;

	return (ebusfsm::Reaction::undefined);
}

void publish(ebusfsm::EbusSequence& eSeq)
{
	std::cout << "publish: " << eSeq.toString().c_str() << std::endl;
}

int main()
{

	ebusfsm::EbusFSM fsm(0xff, "/dev/ttyUSB0", false, nullptr, std::bind(&identify, std::placeholders::_1),
		std::bind(&publish, std::placeholders::_1));

	int count = 0;

	while (count < 100)
	{
		sleep(1);
		std::cout << "main loop - count: " << count << std::endl;

		count++;
	}

	return (0);

}
