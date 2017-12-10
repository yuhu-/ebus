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

#include <EbusCommon.h>
#include <EbusSequence.h>

#include <iostream>
#include <iomanip>

void decode_encode(int type, std::vector<unsigned char> source)
{
	float value = ebusfsm::decode(type, source);

	std::vector<unsigned char> result = ebusfsm::encode(type, value);

	std::cout << "source " << ebusfsm::Sequence::toString(source) << " encode " << ebusfsm::Sequence::toString(result)
		<< " decode " << value << std::endl;
}

void check_range(int type, std::vector<unsigned char> source)
{
	float value = ebusfsm::decode(type, source);

	std::vector<unsigned char> result = ebusfsm::encode(type, value);

	if (source != result)
		std::cout << "differ: source " << ebusfsm::Sequence::toString(source) << " encode "
			<< ebusfsm::Sequence::toString(result) << " decode " << value << std::endl;
}

int main()
{
	std::vector<unsigned char> b1 =
	{ 0x00, 0x01, 0x64, 0x7f, 0x80, 0x81, 0xc8, 0xfe, 0xff };

	std::vector<unsigned char> b2 =
	{ 0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0x00, 0xff, 0xf0, 0xff, 0x00, 0x80, 0x01, 0x80, 0xff, 0x7f, 0x65, 0x02 };

	// BCD
	std::cout << std::endl << "Examples BCD (type=0)" << std::endl;

	for (int i = 0; i < 9; i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(0, source);
	}

	// DATA1b
	std::cout << std::endl << "Examples DATA1b (type=1)" << std::endl;

	for (int i = 0; i < 9; i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(1, source);
	}

	// DATA1c
	std::cout << std::endl << "Examples DATA1c (type=2)" << std::endl;

	for (int i = 0; i < 9; i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(2, source);
	}

	// DATA2b
	std::cout << std::endl << "Examples DATA2b (type=3)" << std::endl;

	for (int i = 0; i < 18; i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(3, source);
	}

	// DATA2c
	std::cout << std::endl << "Examples DATA2c (type=4)" << std::endl;

	for (int i = 0; i < 18; i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(4, source);
	}

	std::cout << std::endl << "Check range BCD (type=0)" << std::endl;

	for (int high = 0x00; high <= 0x09; high++)
	{
		for (int low = 0x00; low <= 0x09; low++)
		{
			std::vector<unsigned char> source(1, (high << 4) + low);

			check_range(0, source);
		}
	}

	std::cout << std::endl << "Check range DATA1b (type=1)" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(1, source);
	}

	std::cout << std::endl << "Check range DATA1c (type=2)" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(2, source);
	}

	std::cout << std::endl << "Check range DATA2b (type=3)" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(3, source);
		}
	}

	std::cout << std::endl << "Check range DATA2c (type=4)" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(4, source);
		}
	}

	return (0);
}
