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

#include <EbusCommon.h>
#include <EbusSequence.h>

#include <iostream>
#include <iomanip>

void decode_encode(const ebusfsm::Type type, std::vector<unsigned char> source)
{
	float value = ebusfsm::decode(type, source);

	std::vector<unsigned char> result = ebusfsm::encode(type, value);

	std::cout << "source " << ebusfsm::Sequence::toString(source) << " encode " << ebusfsm::Sequence::toString(result)
		<< " decode " << value << std::endl;
}

void check_range(const ebusfsm::Type type, std::vector<unsigned char> source)
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
	std::cout << std::endl << "Examples BCD" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(ebusfsm::Type::bcd, source);
	}

	// DATA1b
	std::cout << std::endl << "Examples DATA1b" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(ebusfsm::Type::data1b, source);
	}

	// DATA1c
	std::cout << std::endl << "Examples DATA1c" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(ebusfsm::Type::data1c, source);
	}

	// unsigned char
	std::cout << std::endl << "Examples unsigned char" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(ebusfsm::Type::uchar, source);
	}

	// signed char
	std::cout << std::endl << "Examples signed char" << std::endl;

	for (size_t i = 0; i < b1.size(); i++)
	{
		std::vector<unsigned char> source(1, b1[i]);

		decode_encode(ebusfsm::Type::schar, source);
	}

	// DATA2b
	std::cout << std::endl << "Examples DATA2b" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(ebusfsm::Type::data2b, source);
	}

	// DATA2c
	std::cout << std::endl << "Examples DATA2c" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(ebusfsm::Type::data2c, source);
	}

	// unsigned int
	std::cout << std::endl << "Examples unsigned int" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(ebusfsm::Type::uint, source);
	}

	// signed int
	std::cout << std::endl << "Examples signed int" << std::endl;

	for (size_t i = 0; i < b2.size(); i += 2)
	{
		std::vector<unsigned char> source(&b2[i], &b2[i + 2]);

		decode_encode(ebusfsm::Type::sint, source);
	}

	// BCD
	std::cout << std::endl << "Check range BCD" << std::endl;

	for (int high = 0x00; high <= 0x09; high++)
	{
		for (int low = 0x00; low <= 0x09; low++)
		{
			std::vector<unsigned char> source(1, (high << 4) + low);

			check_range(ebusfsm::Type::bcd, source);
		}
	}

	// DATA1b
	std::cout << std::endl << "Check range DATA1b" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(ebusfsm::Type::data1b, source);
	}

	// DATA1c
	std::cout << std::endl << "Check range DATA1c" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(ebusfsm::Type::data1c, source);
	}

	// unsinged char
	std::cout << std::endl << "Check range unsigned char" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(ebusfsm::Type::uchar, source);
	}

	// signed char
	std::cout << std::endl << "Check range signed char" << std::endl;

	for (int low = 0x00; low <= 0xff; low++)
	{
		std::vector<unsigned char> source(1, low);

		check_range(ebusfsm::Type::schar, source);
	}

	// DATA2b
	std::cout << std::endl << "Check range DATA2b" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(ebusfsm::Type::data2b, source);
		}
	}

	// DATA2c
	std::cout << std::endl << "Check range DATA2c" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(ebusfsm::Type::data2c, source);
		}
	}

	// unsigned int
	std::cout << std::endl << "Check range unsinged int" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(ebusfsm::Type::uint, source);
		}
	}

	// signed int
	std::cout << std::endl << "Check range signed int" << std::endl;

	for (int high = 0x00; high <= 0xff; high++)
	{
		for (int low = 0x00; low <= 0xff; low++)
		{
			std::vector<unsigned char> source
			{ (unsigned char) low, (unsigned char) high };

			check_range(ebusfsm::Type::sint, source);
		}
	}

	return (0);
}
