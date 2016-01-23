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

#include "EbusSequence.h"
#include <iostream>
#include <iomanip>

using std::cout;
using std::endl;

int main()
{
	Sequence seq;

	// TEST FULL
	const unsigned char bytes[] =
		{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43, 0x00,
			0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0, 0x00 };

	for (size_t i = 0; i < sizeof(bytes); i++)
		seq.push_back(bytes[i]);

	EbusSequence full(seq);
	cout << "seq: " << seq.toString() << " Full: " << full.toStringFull() << endl;

	// TEST MASTER
	EbusSequence master;
	master.createMaster("ff52b509030d0600");
	cout << "seq: ff52b509030d0600" << "   Master: " << master.toStringMaster() << endl;

	seq.clear();
	for (size_t i = 0; i < 9; i++)
		seq.push_back(bytes[i]);

	EbusSequence master2;
	master2.createMaster(seq);
	cout << "seq: " << seq.toString() << " Master: " << master2.toStringMaster() << endl;

	// TEST SLAVE
	EbusSequence slave;
	slave.createSlave("03b0fbaa");
	cout << "seq: 03b0fbaa" << "   Slave: " << slave.toStringSlave() << endl;

	seq.clear();
	for (size_t i = 10; i < sizeof(bytes)-1; i++)
		seq.push_back(bytes[i]);

	EbusSequence slave2;
	slave2.createSlave(seq);
	cout << "seq: " << seq.toString() << " Slave: " << slave2.toStringSlave() << endl;

	return (0);
}
