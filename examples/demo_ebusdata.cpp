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
	{ 0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0x00, 0xff, 0xf0, 0xff, 0x00, 0x80, 0x01, 0x80, 0xff, 0x7f, 0x65, 0x02, 0x77, 0x02 };

	// BCD
	std::cout << std::endl << "Examples BCD (type=10)" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(10, source);
	}

	// DATA1b
	std::cout << std::endl << "Examples DATA1b (type=11)" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(11, source);
	}

	// DATA1c
	std::cout << std::endl << "Examples DATA1c (type=12)" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(12, source);
	}

	// unsigned char
	std::cout << std::endl << "Examples unsigned char (type=13)" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(13, source);
	}

	// signed char
	std::cout << std::endl << "Examples signed char (type=14)" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(14, source);
	}

	// DATA2b
	std::cout << std::endl << "Examples DATA2b (type=21)" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(21, source);
	}

	// DATA2c
	std::cout << std::endl << "Examples DATA2c (type=22)" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(22, source);
	}

	// unsigned int
	std::cout << std::endl << "Examples unsigned int (type=23)" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(23, source);
	}

	// signed int
	std::cout << std::endl << "Examples signed int (type=24)" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(24, source);
	}

	// BCD
	std::cout << std::endl << "Check range BCD (type=10)" << std::endl;

	for (int high = 0x00; high <= 0x09; high++)
	{
		for (int low = 0x00; low <= 0x09; low++)
		{
			std::vector<unsigned char> source(1, (high << 4) + low);

			check_range(10, source);
		}
	}

	// DATA1b
	std::cout << std::endl << "Check range DATA1b (type=11)" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(11, source);
	}

	// DATA1c
	std::cout << std::endl << "Check range DATA1c (type=12)" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(12, source);
	}

	// unsinged char
	std::cout << std::endl << "Check range unsigned char (type=13)" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(13, source);
	}

	// signed char
	std::cout << std::endl << "Check range signed char (type=14)" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(14, source);
	}

	// DATA2b
	std::cout << std::endl << "Check range DATA2b (type=21)" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(21, source);
		}
	}

	// DATA2c
	std::cout << std::endl << "Check range DATA2c (type=22)" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(22, source);
		}
	}

	// unsigned int
	std::cout << std::endl << "Check range unsinged int (type=23)" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(23, source);
		}
	}

	// signed int
	std::cout << std::endl << "Check range signed int (type=24)" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(24, source);
		}
	}

	return (0);
}
