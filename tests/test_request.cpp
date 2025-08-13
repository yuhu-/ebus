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

#include <iostream>
#include <string>
#include <vector>

#include "Common.hpp"
#include "Request.hpp"
#include "Sequence.hpp"

struct TestCase {
  bool enabled;
  uint8_t address;
  std::string description;
  std::string sequence;
};

ebus::Request request;

// Helper to run a test with a given hex string and description
void run_test(const TestCase &tc) {
  std::cout << std::endl
            << "=== Test: " << tc.description << " ===" << std::endl;

  // Prepare test sequence from the provided hex string
  std::string tmp = "aaaaaa" + tc.sequence + "aaaaaa";
  ebus::Sequence seq;
  seq.assign(ebus::to_vector(tmp));

  bool requestPending = true;

  ebus::RequestResult testResult;

  std::cout << " address: " << ebus::to_string(tc.address) << std::endl;
  std::cout << "sequence: " << seq.to_string() << std::endl;

  for (size_t i = 0; i < seq.size(); ++i) {
    uint8_t byte = seq[i];

    ebus::RequestState state = request.getState();

    std::cout << "->  read: " << ebus::to_string(byte)
              << "   state: " << ebus::getRequestStateText(state)
              << "\tlockCounter: "
              << static_cast<int>(request.getLockCounter());

    testResult = request.run(byte);

    std::cout << "\tresult: " << ebus::getRequestResultText(testResult);

    if (requestPending && request.requestBus(tc.address))
      requestPending = false;

    if (state != request.getState())
      std::cout << "\tswitch: "
                << ebus::getRequestStateText(request.getState());

    // simulate request bus timer
    if (request.busRequestPending()) {
      std::cout << "\tISR - write address";
      request.busRequestCompleted();
    }

    std::cout << std::endl;
  }

  request.reset();

  std::cout << "--- Test: " << tc.description << " ---" << std::endl;
}

void printCounter() {
  ebus::Request::Counter counter = request.getCounter();

  std::cout << "requestsStartBit:    " << counter.requestsStartBit << std::endl;
  std::cout << "requestsFirstSyn:    " << counter.requestsFirstSyn << std::endl;
  std::cout << "requestsFirstWon:    " << counter.requestsFirstWon << std::endl;
  std::cout << "requestsFirstRetry:  " << counter.requestsFirstRetry
            << std::endl;
  std::cout << "requestsFirstLost:   " << counter.requestsFirstLost
            << std::endl;
  std::cout << "requestsFirstError:  " << counter.requestsFirstError
            << std::endl;
  std::cout << "requestsRetrySyn:    " << counter.requestsRetrySyn << std::endl;
  std::cout << "requestsRetryError:  " << counter.requestsRetryError
            << std::endl;
  std::cout << "requestsSecondWon:   " << counter.requestsSecondWon
            << std::endl;
  std::cout << "requestsSecondLost:  " << counter.requestsSecondLost
            << std::endl;
  std::cout << "requestsSecondError: " << counter.requestsSecondError
            << std::endl;
}

// clang-format off
std::vector<TestCase> test_cases = {
    {true, 0x33, "Normal", "33feb5050427002d00"},
    {true, 0x33, "Syn byte - Normal", "aa33feb5050427002d00"},
    {true, 0x33, "Wrong byte", "5c"},
    {true, 0x33, "Priority lost", "01feb5050427002d007b"},
    {true, 0x33, "Priority lost/wrong byte", "01ab"},
    {true, 0x33, "Priority fit/won", "73aa33feb5050427002d00"},
    {true, 0x33, "Priority fit/lost", "73aa13"},
    {true, 0x33, "Priority fit/error", "73aac5"},
    {true, 0x33, "Priority retry/error", "73a0"},
    {true, 0x30, "Priority fit - Sub lost", "1052b50401314b000200002c00"},
};
// clang-format on

int main() {
  for (const TestCase &tc : test_cases)
    if (tc.enabled) run_test(tc);

  printCounter();

  return EXIT_SUCCESS;
}
