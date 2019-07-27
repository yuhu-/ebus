/*
 * Copyright (C) Roland Jax 2012-2019 <roland.jax@liwest.at>
 *
 * This file is part of ebus.
 *
 * ebus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebus. If not, see http://www.gnu.org/licenses/.
 */

#include <cstddef>
#include <iostream>
#include <string>

#include "../src/Sequence.h"
#include "../src/Telegram.h"

int main()
{
	// Test Sequence
	ebus::Sequence source("ff52b509030d0600");
	ebus::Sequence part1("b509");

	std::cout << "find '" << part1.toString() << "' in '" << source.toString() << "'";

	if (source.find(part1) != ebus::Sequence::npos)
		std::cout << " << found at: " << source.find(part1) << std::endl;
	else
		std::cout << " << not found" << std::endl;

	ebus::Sequence part2("0d0601");

	std::cout << "find '" << part2.toString() << "' in '" << source.toString() << "'";

	if (source.find(part2) != ebus::Sequence::npos)
		std::cout << " << found at: " << source.find(part2) << std::endl;
	else
		std::cout << " << not found" << std::endl;

	std::cout << "contains '" << part1.toString() << "' in '" << source.toString() << "'";

	if (source.contains(part1.toString()))
		std::cout << " << yes" << std::endl;
	else
		std::cout << " << no" << std::endl;

	std::cout << "contains '" << part2.toString() << "' in '" << source.toString() << "'";

	if (source.contains(part2.toString()))
		std::cout << " << yes: " << std::endl;
	else
		std::cout << " << no" << std::endl;

	ebus::Sequence test1("ff52b509030d0600");

	std::cout << "compare '" << test1.toString() << "' with '" << source.toString() << "'";

	if (source.compare(test1) == 0)
		std::cout << " << equal" << std::endl;
	else
		std::cout << " << not equal" << std::endl;

	ebus::Sequence test2("ff52b509030d0601");

	std::cout << "compare '" << test2.toString() << "' with '" << source.toString() << "'";

	if (source.compare(test2) == 0)
		std::cout << " << equal" << std::endl;
	else
		std::cout << " << not equal" << std::endl;

	// parse sequence range
	ebus::Sequence tmp("ff12b509030d0000d700037702006100");
	ebus::Telegram parse(tmp);

	std::cout << "range: " << parse.toString() << " slave(1,2) = '" << ebus::Sequence::toString(parse.getSlave().range(1, 2))
		<< "'" << std::endl;

	ebus::Sequence tmp2("ff0ab509030d0e00830002e0028900");
	ebus::Telegram parse2(tmp2);

	std::cout << "range: " << parse2.toString() << " slave(1,2) = '" << ebus::Sequence::toString(parse2.getSlave().range(1, 2))
		<< "'" << std::endl;

	// Test Telegram
	ebus::Sequence seq;

	// Normal
	const std::byte bytes[] =
	{ std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09), std::byte(0x03), std::byte(0x0d), std::byte(0x06),
		std::byte(0x00), std::byte(0x43), std::byte(0x00), std::byte(0x03), std::byte(0xb0), std::byte(0xfb), std::byte(0xa9),
		std::byte(0x01), std::byte(0xd0), std::byte(0x00) };

	for (size_t i = 0; i < sizeof(bytes); i++)
		seq.push_back(bytes[i]);

	ebus::Telegram full(seq);

	std::cout << "seq: " << seq.toString() << " Full: " << full.toString() << std::endl;
	seq.clear();

	// NAK from slave
	const std::byte bytes2[] =
	{ std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09), std::byte(0x03), std::byte(0x0d), std::byte(0x06),
		std::byte(0x00), std::byte(0x43), std::byte(0xff), std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09),
		std::byte(0x03), std::byte(0x0d), std::byte(0x06), std::byte(0x00), std::byte(0x43), std::byte(0x00), std::byte(0x03),
		std::byte(0xb0), std::byte(0xfb), std::byte(0xa9), std::byte(0x01), std::byte(0xd0), std::byte(0x00) };

	for (size_t i = 0; i < sizeof(bytes2); i++)
		seq.push_back(bytes2[i]);

	ebus::Telegram full2(seq);

	std::cout << "seq: " << seq.toString() << " Full2 (NAK from slave): " << full2.toString() << std::endl;
	seq.clear();

	// twice NAK from slave
	const std::byte bytes22[] =
	{ std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09), std::byte(0x03), std::byte(0x0d), std::byte(0x06),
		std::byte(0x00), std::byte(0x43), std::byte(0xff), std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09),
		std::byte(0x03), std::byte(0x0d), std::byte(0x06), std::byte(0x00), std::byte(0x43), std::byte(0xff) };

	for (size_t i = 0; i < sizeof(bytes22); i++)
		seq.push_back(bytes22[i]);

	ebus::Telegram full22(seq);

	std::cout << "seq: " << seq.toString() << " Full22 (twice NAK from slave): " << full22.toString() << std::endl;
	seq.clear();

	// NAK from master
	const std::byte bytes3[] =
	{ std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09), std::byte(0x03), std::byte(0x0d), std::byte(0x06),
		std::byte(0x00), std::byte(0x43), std::byte(0x00), std::byte(0x03), std::byte(0xb0), std::byte(0xfb), std::byte(0xa9),
		std::byte(0x01), std::byte(0xd0), std::byte(0xff), std::byte(0x00), std::byte(0x03), std::byte(0xb0), std::byte(0xfb),
		std::byte(0xa9), std::byte(0x01), std::byte(0xd0), std::byte(0x00) };

	for (size_t i = 0; i < sizeof(bytes3); i++)
		seq.push_back(bytes3[i]);

	ebus::Telegram full3(seq);

	std::cout << "seq: " << seq.toString() << " Full3 (NAK from master): " << full3.toString() << std::endl;
	seq.clear();

	// NAK from slave and master
	const std::byte bytes4[] =
	{ std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09), std::byte(0x03), std::byte(0x0d), std::byte(0x06),
		std::byte(0x00), std::byte(0x43), std::byte(0xff), std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09),
		std::byte(0x03), std::byte(0x0d), std::byte(0x06), std::byte(0x00), std::byte(0x43), std::byte(0x00), std::byte(0x03),
		std::byte(0xb0), std::byte(0xfb), std::byte(0xa9), std::byte(0x01), std::byte(0xd0), std::byte(0xff), std::byte(0x00),
		std::byte(0x03), std::byte(0xb0), std::byte(0xfb), std::byte(0xa9), std::byte(0x01), std::byte(0xd0), std::byte(0x00) };

	for (size_t i = 0; i < sizeof(bytes4); i++)
		seq.push_back(bytes4[i]);

	ebus::Telegram full4(seq);

	std::cout << "seq: " << seq.toString() << " Full4 (NAK from slave and master): " << full4.toString() << std::endl;
	seq.clear();

	// twice NAK from slave and master
	const std::byte bytes44[] =
	{ std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09), std::byte(0x03), std::byte(0x0d), std::byte(0x06),
		std::byte(0x00), std::byte(0x43), std::byte(0xff), std::byte(0xff), std::byte(0x52), std::byte(0xb5), std::byte(0x09),
		std::byte(0x03), std::byte(0x0d), std::byte(0x06), std::byte(0x00), std::byte(0x43), std::byte(0x00), std::byte(0x03),
		std::byte(0xb0), std::byte(0xfb), std::byte(0xa9), std::byte(0x01), std::byte(0xd0), std::byte(0xff), std::byte(0x00),
		std::byte(0x03), std::byte(0xb0), std::byte(0xfb), std::byte(0xa9), std::byte(0x01), std::byte(0xd0), std::byte(0xff) };

	for (size_t i = 0; i < sizeof(bytes44); i++)
		seq.push_back(bytes44[i]);

	ebus::Telegram full44(seq);

	std::cout << "seq: " << seq.toString() << " Full44 (twice NAK from slave and master): " << full44.toString() << std::endl;
	seq.clear();

	// defect sequence
	const std::byte bytes5[] =
	{ std::byte(0x10), std::byte(0x7f), std::byte(0xc2), std::byte(0xb5), std::byte(0x10), std::byte(0x09), std::byte(0x00),
		std::byte(0x02), std::byte(0x40), std::byte(0x00), std::byte(0x00), std::byte(0x00), std::byte(0x00), std::byte(0x00),
		std::byte(0x02), std::byte(0x15) };

	for (size_t i = 0; i < sizeof(bytes5); i++)
		seq.push_back(bytes5[i]);

	ebus::Telegram full5(seq);

	std::cout << "seq: " << seq.toString() << " Full5 (defect sequence): " << full5.toString() << std::endl;
	seq.clear();

	// missing acknowledge byte
	const std::byte bytes6[] =
	{ std::byte(0x10), std::byte(0x08), std::byte(0xb5), std::byte(0x11), std::byte(0x01), std::byte(0x02), std::byte(0x8a),
		std::byte(0xff), std::byte(0x10), std::byte(0x08), std::byte(0xb5), std::byte(0x11), std::byte(0x01), std::byte(0x02),
		std::byte(0x8a), std::byte(0x00), std::byte(0x03), std::byte(0xb0), std::byte(0xfb), std::byte(0xa9), std::byte(0x01),
		std::byte(0xd0), std::byte(0xff), std::byte(0x00), std::byte(0x03), std::byte(0xb0), std::byte(0xfb), std::byte(0xa9),
		std::byte(0x01), std::byte(0xd0) };

	for (size_t i = 0; i < sizeof(bytes6); i++)
		seq.push_back(bytes6[i]);

	ebus::Telegram full6(seq);

	std::cout << "seq: " << seq.toString() << " Full6 (missing acknowledge byte): " << full6.toString() << std::endl;
	seq.clear();

	// create master
	ebus::Telegram master;
	master.createMaster("ff52b509030d0600");
	std::cout << "seq: ff52b509030d0600" << "   Master : " << master.toStringMaster() << std::endl;

	for (size_t i = 0; i < 9; i++)
		seq.push_back(bytes[i]);

	ebus::Telegram master2;
	master2.createMaster(seq);
	std::cout << "seq: " << seq.toString() << " Master2: " << master2.toStringMaster() << std::endl;
	seq.clear();

	// create slave
	master.createSlave("03b0fbaa");
	std::cout << "seq: 03b0fbaa" << "   Slave: " << master.toStringSlave() << std::endl;

	for (size_t i = 10; i < sizeof(bytes) - 1; i++)
		seq.push_back(bytes[i]);

	master2.createSlave(seq);
	std::cout << "seq: " << seq.toString() << " Slave: " << master2.toStringSlave() << std::endl;
	seq.clear();

	return (0);
}
