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

	const unsigned char bytes[] =
		{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43, 0x00,
			0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0, 0x00 };

	for (size_t i = 0; i < sizeof(bytes); i++)
		seq.push_back(bytes[i]);

	EbusSequence eSeq(seq);
	cout << eSeq.printUpdate() << endl;

	EbusSequence eSeq2("ff50b509030d0000");
	cout << eSeq2.printMaster() << endl;

	seq.clear();
	const unsigned char bytes2[] =
		{ 0x0a, 0xb5, 0x45, 0x48, 0x50, 0x30, 0x30, 0x04, 0x16, 0x72,
			0x01, 0xea };

	for (size_t i = 0; i < sizeof(bytes2); i++)
		seq.push_back(bytes2[i]);

	EbusSequence eSeq3;
	eSeq3.createSlave(seq);
	cout << eSeq3.printSlave() << endl;

	return (0);

}
