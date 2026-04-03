/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Core/Request.hpp"
#include "Core/Sequence.hpp"
#include "Utils/Common.hpp"

struct TestCase {
  bool enabled;
  uint8_t address;
  std::string description;
  std::string sequence;
};

ebus::Request request;

// Helper to run a test with a given hex string and description
void run_test(const TestCase& tc) {
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

void printMetrics() {
  auto requestMetrics = request.getMetrics();
  std::cout << "\n--- Request Metrics ---" << std::endl;
  for (auto const& m : requestMetrics) {
    std::cout << std::setw(60) << std::left << m.first << ": " << m.second.last
              << std::endl;
  }
}

// clang-format off
std::vector<TestCase> test_cases = {
    {true, 0x33, "Normal", "33feb5050427002d00"},
    // Immediate SYN (Auto-SYN) scenario
    {true, 0x33, "Syn byte - Normal", "aa33feb5050427002d00"},

    // Reading 0x5c while sending 0x33.
    // 0x33 = 0011 0011
    // 0x5c = 0101 1100
    // This implies we sent '0' (bit 4) but read '1', which is impossible on
    // a Wire-AND bus (0 dominates). Treated as electrical error/wrong byte.
    {true, 0x33, "Wrong byte", "5c"},

    // Priority Loss:
    // We sent 0x33 (Prio Class 3). Read 0x01 (Prio Class 1).
    // Classes differ (3 != 1), so we withdraw (standard loss).
    {true, 0x33, "Priority lost", "01feb5050427002d007b"},
    {true, 0x33, "Priority lost/wrong byte", "01ab"},

    // Priority Fit (Retry Allowed):
    // We sent 0x33. Bus reads 0x13.
    // 0x33 = 0011 0011 (Class 3)
    // 0x13 = 0001 0011 (Class 3)
    // We lost (read 0x13 != 0x33), but Priority Class matches.
    // 0x13 and 0x33 share Priority Class 3. We enter retry.
    // We expect immediate SYN (aa) to retry.
    {true, 0x33, "Priority fit/won", "13aa33feb5050427002d00"},
    // Priority Fit/Lost: We retry at SYN, but lose again to 0x13.
    {true, 0x33, "Priority fit/lost", "13aa13"},
    // Priority Fit/Error: We retry at SYN, but see garbage (C5).
    {true, 0x33, "Priority fit/error", "13aac5"},
    // Sub lost: We are 0x30. Bus is 0x10. Prio 0 match.
    {true, 0x30, "Priority fit - Sub lost", "1052b50401314b000200002c00"},
};
// clang-format on

int main() {
  for (const TestCase& tc : test_cases)
    if (tc.enabled) run_test(tc);

  printMetrics();

  return EXIT_SUCCESS;
}
