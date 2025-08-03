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
  ebus::RequestResult expectedResult;
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

  request.setLockCounter(3);
  bool requestFlag = true;

  ebus::RequestResult testResult;

  std::cout << " address: " << ebus::to_string(tc.address) << std::endl;
  std::cout << "sequence: " << seq.to_string() << std::endl;

  for (size_t i = 0; i < seq.size(); ++i) {
    uint8_t byte = seq[i];

    std::cout << "->  read: " << ebus::to_string(byte)
              << " lockCounter: " << static_cast<int>(request.getLockCounter())
              << std::endl;

    if (request.getLockCounter() == 0 && requestFlag) {
      ebus::RequestState state = request.getState();
      std::cout << " request: " << ebus::getRequestStateText(state)
                << " address: " << ebus::to_string(tc.address)
                << " byte:" << ebus::to_string(byte) << std::endl;

      testResult = request.run(tc.address, byte);

      std::cout << "  result: " << ebus::getRequestResultText(testResult)
                << std::endl;

      if (state != request.getState()) {
        std::cout << "  switch: "
                  << ebus::getRequestStateText(request.getState()) << std::endl;
      }

      switch (testResult) {
        case ebus::RequestResult::firstWon:
        case ebus::RequestResult::firstLost:
        case ebus::RequestResult::firstError:
        case ebus::RequestResult::retryError:
        case ebus::RequestResult::secondWon:
        case ebus::RequestResult::secondLost:
        case ebus::RequestResult::secondError:
          requestFlag = false;  // Stop further requests
          break;

        default:
          break;
      }
    }

    // request.handleLockCounter(byte);
  }

  std::string resultText;

  if (testResult != tc.expectedResult) {
    resultText += "failed - expected ";
    resultText += ebus::getRequestResultText(tc.expectedResult);
    resultText += ", got ";
    resultText += ebus::getRequestResultText(testResult);
  } else {
    resultText += "passed";
  }

  std::cout << "--- Test: " << resultText << " ---" << std::endl;
}

void printCounter() {
  ebus::Request::Counter counter = request.getCounter();

  std::cout << std::endl
            << "requestsTotal:       " << counter.requestsTotal << std::endl;
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
    {true, 0x33, "Normal", "33feb5050427002d00", ebus::RequestResult::firstWon},
    {true, 0x33, "Syn byte - Normal", "aa33feb5050427002d00", ebus::RequestResult::firstWon},
    {true, 0x33, "Wrong byte", "5c", ebus::RequestResult::firstError},
    {true, 0x33, "Priority lost", "01feb5050427002d007b", ebus::RequestResult::firstLost},
    {true, 0x33, "Priority lost/wrong byte", "01ab", ebus::RequestResult::firstLost},
    {true, 0x33, "Priority fit/won", "73aa33feb5050427002d00", ebus::RequestResult::secondWon},
    {true, 0x33, "Priority fit/lost", "73aa13", ebus::RequestResult::secondLost},
    {true, 0x33, "Priority fit/error", "73aac5", ebus::RequestResult::secondError},
    {true, 0x33, "Priority retry/error", "73a0", ebus::RequestResult::retryError},
    {true, 0x30, "Priority fit - Sub lost", "1052b50401314b000200002c00", ebus::RequestResult::firstLost},
};
// clang-format on

int main() {
  for (const TestCase &tc : test_cases)
    if (tc.enabled) run_test(tc);

  printCounter();

  return EXIT_SUCCESS;
}
