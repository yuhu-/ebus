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

#ifndef EBUSFSM_EBUSCOMMON_H
#define EBUSFSM_EBUSCOMMON_H

#include <string>
#include <vector>
#include <cstddef>

namespace ebusfsm
{

std::byte calcCRC(const std::byte byte, const std::byte init);

bool isMaster(const std::byte byte);

bool isSlave(const std::byte byte);

bool isAddressValid(const std::byte byte);

std::byte slaveAddress(const std::byte masterAddress);

bool isHex(const std::string& str, std::ostringstream& result, const int& nibbles);

} // namespace ebusfsm

#endif // EBUSFSM_EBUSCOMMON_H

