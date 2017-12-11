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

#include <EbusSequence.h>
#include <EbusCommon.h>

#include <iostream>
#include <iomanip>

int main()
{
	// Test Sequence
	ebusfsm::Sequence source("ff52b509030d0600");
	ebusfsm::Sequence part1("b509");

	std::cout << "find '" << part1.toString() << "' in '" << source.toString() << "'";

	if (source.find(part1) != ebusfsm::Sequence::npos)
		std::cout << " << found at: " << source.find(part1) << std::endl;
	else
		std::cout << " << not found" << std::endl;

	ebusfsm::Sequence part2("0d0601");

	std::cout << "find '" << part2.toString() << "' in '" << source.toString() << "'";

	if (source.find(part2) != ebusfsm::Sequence::npos)
		std::cout << " << found at: " << source.find(part2) << std::endl;
	else
		std::cout << " << not found" << std::endl;

	ebusfsm::Sequence test1("ff52b509030d0600");

	std::cout << "compare '" << test1.toString() << "' with '" << source.toString() << "'";

	if (source.compare(test1) == 0)
		std::cout << " << equal" << std::endl;
	else
		std::cout << " << not equal" << std::endl;

	ebusfsm::Sequence test2("ff52b509030d0601");

	std::cout << "compare '" << test2.toString() << "' with '" << source.toString() << "'";

	if (source.compare(test2) == 0)
		std::cout << " << equal" << std::endl;
	else
		std::cout << " << not equal" << std::endl;

	// Test EbusSequence
	ebusfsm::Sequence seq;

	// Normal
	const unsigned char bytes[] =
	{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0x00 };							// master

	for (size_t i = 0; i < sizeof(bytes); i++)
		seq.push_back(bytes[i]);

	ebusfsm::EbusSequence full(seq);

	std::cout << "seq: " << seq.toString() << " Full: " << full.toString() << std::endl;
	seq.clear();

	// NAK from slave
	const unsigned char bytes2[] =
	{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0xff,								// slave NAK
	  0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0x00 };							// master

	for (size_t i = 0; i < sizeof(bytes2); i++)
		seq.push_back(bytes2[i]);

	ebusfsm::EbusSequence full2(seq);

	std::cout << "seq: " << seq.toString() << " Full2 (NAK from slave): " << full2.toString() << std::endl;
	seq.clear();

	// twice NAK from slave
	const unsigned char bytes22[] =
	{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0xff,								// slave NAK
	  0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0xff };							// slave

	for (size_t i = 0; i < sizeof(bytes22); i++)
		seq.push_back(bytes22[i]);

	ebusfsm::EbusSequence full22(seq);

	std::cout << "seq: " << seq.toString() << " Full22 (twice NAK from slave): " << full22.toString() << std::endl;
	seq.clear();

	// NAK from master
	const unsigned char bytes3[] =
	{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0xff,								// master NAK
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0x00 };							// master

	for (size_t i = 0; i < sizeof(bytes3); i++)
		seq.push_back(bytes3[i]);

	ebusfsm::EbusSequence full3(seq);

	std::cout << "seq: " << seq.toString() << " Full3 (NAK from master): " << full3.toString() << std::endl;
	seq.clear();

	// NAK from slave and master
	const unsigned char bytes4[] =
	{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0xff,								// slave NAK
	  0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0xff,								// master NAK
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0x00 };							// master

	for (size_t i = 0; i < sizeof(bytes4); i++)
		seq.push_back(bytes4[i]);

	ebusfsm::EbusSequence full4(seq);

	std::cout << "seq: " << seq.toString() << " Full4 (NAK from slave and master): " << full4.toString() << std::endl;
	seq.clear();

	// twice NAK from slave and master
	const unsigned char bytes44[] =
	{ 0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0xff,								// slave NAK
	  0xff, 0x52, 0xb5, 0x09, 0x03, 0x0d, 0x06, 0x00, 0x43,		// master
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0xff,								// master NAK
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0xff };							// master

	for (size_t i = 0; i < sizeof(bytes44); i++)
		seq.push_back(bytes44[i]);

	ebusfsm::EbusSequence full44(seq);

	std::cout << "seq: " << seq.toString() << " Full44 (twice NAK from slave and master): " << full44.toString() << std::endl;
	seq.clear();

	// defect sequence
	const unsigned char bytes5[] =
	{ 0x10, 0x7f, 0xc2, 0xb5, 0x10, 0x09, 0x00, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x15 };

	for (size_t i = 0; i < sizeof(bytes5); i++)
		seq.push_back(bytes5[i]);

	ebusfsm::EbusSequence full5(seq);

	std::cout << "seq: " << seq.toString() << " Full5 (defect sequence): " << full5.toString() << std::endl;
	seq.clear();

	// missing acknowledge byte
	const unsigned char bytes6[] =
	{ 0x10, 0x08, 0xb5, 0x11, 0x01, 0x02, 0x8a,			// master
	  0xff,								// slave NAK
	  0x10, 0x08, 0xb5, 0x11, 0x01, 0x02, 0x8a,			// master
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0,			// slave
	  0xff,								// master NAK
	  0x00, 0x03, 0xb0, 0xfb, 0xa9, 0x01, 0xd0 };  			// slave

	for (size_t i = 0; i < sizeof(bytes6); i++)
		seq.push_back(bytes6[i]);

	ebusfsm::EbusSequence full6(seq);

	std::cout << "seq: " << seq.toString() << " Full6 (missing acknowledge byte): " << full6.toString() << std::endl;
	seq.clear();

	// create master
	ebusfsm::EbusSequence master;
	master.createMaster("ff52b509030d0600");
	std::cout << "seq: ff52b509030d0600" << "   Master : " << master.toStringMaster() << std::endl;

	for (size_t i = 0; i < 9; i++)
		seq.push_back(bytes[i]);

	ebusfsm::EbusSequence master2;
	master2.createMaster(seq);
	std::cout << "seq: " << seq.toString() << " Master2: " << master2.toStringMaster() << std::endl;
	seq.clear();

	// create slave
	ebusfsm::EbusSequence slave;
	slave.createSlave("03b0fbaa");
	std::cout << "seq: 03b0fbaa" << "   Slave: " << slave.toStringSlave() << std::endl;

	for (size_t i = 10; i < sizeof(bytes) - 1; i++)
		seq.push_back(bytes[i]);

	ebusfsm::EbusSequence slave2;
	slave2.createSlave(seq);
	std::cout << "seq: " << seq.toString() << " Slave: " << slave2.toStringSlave() << std::endl;
	seq.clear();

	// parse sequence range
	ebusfsm::Sequence tmp("ff12b509030d0000d700037702006100");
	ebusfsm::EbusSequence parse(tmp);

	std::cout << "parse: " << parse.toString() << " parse(" << ebusfsm::Sequence::toString(parse.getSlave().range(1, 2)) << ") => "
		<< ebusfsm::decode(4, parse.getSlave().range(1, 2)) << std::endl;

	return (0);
}
