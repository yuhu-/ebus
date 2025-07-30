/*
 * Copyright (C) 2025 Roland Jax
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

#include "Request.hpp"

#include "Common.hpp"

ebus::Request::Request() {
  stateRequests = {&Request::firstTry, &Request::retrySyn, &Request::secondTry};
}

ebus::RequestState ebus::Request::getState() const { return state; }

ebus::RequestResult ebus::Request::run(const uint8_t &address,
                                       const uint8_t &byte) {
  size_t idx = static_cast<size_t>(state);
  if (idx < stateRequests.size() && stateRequests[idx])
    (this->*stateRequests[idx])(address, byte);

  return result;
}

void ebus::Request::firstTry(const uint8_t &address, const uint8_t &byte) {
  if (byte == sym_syn) {
    result = RequestResult::firstSyn;
  } else if (byte == address) {
    result = RequestResult::firstWon;
  } else if (isMaster(byte)) {
    if (checkPriorityClassSubAddress(address, byte)) {
      state = RequestState::retrySyn;
      result = RequestResult::firstRetry;
    } else {
      result = RequestResult::firstLost;
    }
  } else {
    result = RequestResult::firstError;
  }
}

void ebus::Request::retrySyn(const uint8_t &address, const uint8_t &byte) {
  if (byte == sym_syn) {
    state = RequestState::secondTry;
    result = RequestResult::retrySyn;
  } else {
    state = RequestState::firstTry;
    result = RequestResult::retryError;
  }
}

void ebus::Request::secondTry(const uint8_t &address, const uint8_t &byte) {
  if (byte == address) {
    result = RequestResult::secondWon;
  } else if (isMaster(byte)) {
    result = RequestResult::secondLost;
  } else {
    result = RequestResult::secondError;
  }
  state = RequestState::firstTry;
}

// check priority class (lower nibble) and sub address (higher nibble)
bool ebus::Request::checkPriorityClassSubAddress(const uint8_t &address,
                                                 const uint8_t &byte) {
  return (byte & 0x0f) == (address & 0x0f) &&  // priority class
         (byte & 0xf0) > (address & 0xf0);     // sub address
}
