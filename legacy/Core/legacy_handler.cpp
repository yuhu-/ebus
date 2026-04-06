/*
 * Copyright (C) 2023-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <cstddef>
#include <ebus/Datatypes.hpp>
#include <ebus/Metrics.hpp>
#include <iomanip>
#include <iostream>
#include <string>

#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Core/Sequence.hpp"
#include "Core/Telegram.hpp"
#include "Platform/Bus.hpp"
#include "TestUtils.hpp"
#include "Utils/Common.hpp"

struct TestCase {
  ebus::MessageType messageType;
  uint8_t address;
  std::string description;
  std::string read_string;
  std::string send_string = "";
  struct ExpectedResult {
    int telegram;
    int errors;
  } expected;
};

std::atomic<int> g_error_count(0);
std::atomic<int> g_telegram_count(0);
bool g_detailed_output = false;

void printByte(const std::string& prefix, const uint8_t& byte,
               const std::string& postfix) {
  if (!g_detailed_output) return;
  std::cout << prefix << ebus::to_string(byte) << " " << postfix << std::endl;
}
ebus::Request request;

ebus::busConfig config = {.device = "/dev/simulation", .simulate = true};
ebus::RuntimeConfig runtime = {.address = 0x33, .window = 50, .offset = 5};
ebus::Bus bus(config, runtime, &request);

ebus::Handler handler(runtime.address, &bus, &request);

void readFunction(const uint8_t& byte) {
  if (!g_detailed_output) return;
  std::cout << "->  read: " << ebus::to_string(byte) << std::endl;
}

void busRequestWonCallback() {
  if (g_detailed_output) std::cout << " request: won" << std::endl;
}

void busRequestLostCallback() {
  if (g_detailed_output) std::cout << " request: lost" << std::endl;
}

void reactiveMasterSlaveCallback(const std::vector<uint8_t>& master,
                                 std::vector<uint8_t>* const slave) {
  std::vector<uint8_t> search;
  search = {0x07, 0x04};  // 0008070400
  if (ebus::contains(master, search))
    *slave = ebus::to_vector("0ab5504d53303001074302");
  search = {0x07, 0x05};  // 0008070500
  if (ebus::contains(master, search))
    *slave = ebus::to_vector("0ab5504d533030010743");  // defect

  if (!g_detailed_output) return;
  std::cout << "reactive: " << ebus::to_string(master) << " "
            << ebus::to_string(*slave) << std::endl;
}

void telegramCallback(const ebus::MessageType& messageType,
                      const ebus::TelegramType& telegramType,
                      const std::vector<uint8_t>& master,
                      const std::vector<uint8_t>& slave) {
  g_telegram_count++;
  if (!g_detailed_output) return;
  switch (telegramType) {
    case ebus::TelegramType::broadcast:
      std::cout << "    type: broadcast" << std::endl;
      break;
    case ebus::TelegramType::master_master:
      std::cout << "    type: master master" << std::endl;
      break;
    case ebus::TelegramType::master_slave:
      std::cout << "    type: master slave" << std::endl;
      break;
  }
  switch (messageType) {
    case ebus::MessageType::active:
      std::cout << "  active: ";
      break;
    case ebus::MessageType::passive:
      std::cout << " passive: ";
      break;
    case ebus::MessageType::reactive:
      std::cout << "reactive: ";
      break;
  }
  std::cout << ebus::to_string(master) << " " << ebus::to_string(slave)
            << std::endl;
}

void errorCallback(const std::string& error, const std::vector<uint8_t>& master,
                   const std::vector<uint8_t>& slave) {
  g_error_count++;
  if (!g_detailed_output) return;
  std::cout << "   error: " << error << " master '" << ebus::to_string(master)
            << "' slave '" << ebus::to_string(slave) << "'" << std::endl;
}

void printMetrics() {
  auto handlerMetrics = handler.getMetrics();
  std::cout << "\n--- Handler Metrics ---" << std::endl;
  for (auto const& m : handlerMetrics) {
    std::cout << std::setw(60) << std::left << m.first << ": " << m.second.last
              << std::endl;
  }

  auto requestMetrics = request.getMetrics();
  std::cout << "\n--- Request Metrics ---" << std::endl;
  for (auto const& m : requestMetrics) {
    std::cout << std::setw(60) << std::left << m.first << ": " << m.second.last
              << std::endl;
  }

  auto busMetrics = bus.getMetrics();
  std::cout << "\n--- Bus Metrics ---" << std::endl;
  for (auto const& m : busMetrics) {
    std::cout << std::setw(60) << std::left << m.first << ": " << m.second.last
              << std::endl;
  }
}

bool run_test(const TestCase& tc) {
  if (g_detailed_output)
    std::cout << std::endl
              << "=== Test: " << tc.description << " ===" << std::endl;
  g_error_count = 0;
  g_telegram_count = 0;

  request.reset();
  handler.reset();

  handler.setSourceAddress(tc.address);

  // Prepare test sequence from the provided hex string
  std::string tmp = "aaaaaa" + tc.read_string + "aaaaaa";
  ebus::Sequence seq;
  seq.assign(ebus::to_vector(tmp));

  if (tc.send_string.size() > 0)
    handler.sendActiveMessage(ebus::to_vector(tc.send_string));

  bool busRequestFlag = false;
  for (size_t i = 0; i < seq.size(); i++) {
    switch (handler.getState()) {
      // These states consume the echoed byte of what was just written
      case ebus::HandlerState::releaseBus:
        i--;
        break;
      default:
        readFunction(seq[i]);
        break;
    }

    if (busRequestFlag) {
      request.busRequestCompleted();
      busRequestFlag = false;
    }
    request.run(seq[i]);
    handler.run({seq[i], request.getState(), request.getResult(),
                 request.getLockCounter(), std::chrono::steady_clock::now()});

    // simulte request bus timer
    if (seq[i] == ebus::sym_syn && request.busRequestPending()) {
      if (g_detailed_output) std::cout << " ISR - write address" << std::endl;
      bus.writeByte(request.busRequestAddress());
      busRequestFlag = true;
    }
  }

  request.reset();
  handler.reset();

  bool pass = (g_error_count == tc.expected.errors &&
               g_telegram_count == tc.expected.telegram);

  if (g_detailed_output) {
    std::cout << "--- Test: " << tc.description << " ---" << std::endl
              << "[RESULT] ";
    if (pass) {
      std::cout << "PASSED" << std::endl;
    } else {
      std::cout << "FAILED. Expected " << tc.expected.telegram
                << " telegram and " << tc.expected.errors << " errors, but got "
                << g_telegram_count << " and " << g_error_count << "."
                << std::endl;
    }
  } else {
    std::cout << "[TEST] " << tc.description << ": "
              << (pass ? "PASSED" : "FAILED") << std::endl;
  }
  return pass;
}

// clang-format off
std::vector<TestCase> test_cases = {
  {ebus::MessageType::passive, 0x33, "passive MS: Normal", "ff52b509030d0600430003b0fba901d000", "", {1, 0}},
  {ebus::MessageType::passive, 0x33, "passive MS: Master CRC error -> NAK -> Retry OK", "ff52b509030d060044ffff52b509030d0600430003b0fba901d000", "", {1, 0}},
  {ebus::MessageType::passive, 0x33, "passive MS: Master valid -> NAK -> Retry OK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d000", "", {1, 0}},
  {ebus::MessageType::passive, 0x33, "passive MS: Master valid -> NAK -> Retry valid -> NAK", "ff52b509030d060043ffff52b509030d060043ff", "", {0, 1}},
  {ebus::MessageType::passive, 0x33, "passive MS: Slave CRC error -> NAK -> Retry OK", "ff52b509030d0600430003b0fba902d0ff03b0fba901d000", "", {1, 1}},
  {ebus::MessageType::passive, 0x33, "passive MS: Slave valid -> NAK -> Retry valid -> NAK", "ff52b509030d0600430003b0fba901d0ff03b0fba901d0ff", "", {0, 1}},
  {ebus::MessageType::passive, 0x33, "passive MS: Master NAK/Retry OK - Slave NAK/Retry OK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d000", "", {1, 0}},
  {ebus::MessageType::passive, 0x33, "passive MS: Master NAK/Retry OK - Slave NAK/Retry NAK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d0ff", "", {0, 1}},
  {ebus::MessageType::passive, 0x33, "passive MM: Normal", "1000b5050427002400d900", "", {1, 0}},
  {ebus::MessageType::passive, 0x33, "passive BC: Malformed sequence", "00fe0704003c", "", {0, 1}},
  {ebus::MessageType::passive, 0x33, "passive 00: Reset", "00", "", {0, 1}},
  {ebus::MessageType::passive, 0x33, "passive 0704: Scan", "002e0704004e", "", {0, 1}},
  {ebus::MessageType::passive, 0x33, "passive BC: Normal", "10fe07000970160443183105052592", "", {1, 0}},
  {ebus::MessageType::passive, 0x33, "passive MS: Slave CRC 0x21 is missing", "1008b5130304cd017f000acd01000000000100010000", "", {0, 2}},

  {ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/ACK", "0038070400ab000ab5504d5330300107430245ff0ab5504d533030010743024600", "", {1, 0}},
  {ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/NAK", "0038070400ab000ab5504d5330300107430245ff0ab5504d5330300107430245ff", "", {0, 1}},
  {ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/correct", "0038070400acff0038070400ab000ab5504d533030010743024600", "", {1, 1}},
  {ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/defect", "0038070400ff0038070400acff", "", {0, 2}},
  {ebus::MessageType::reactive, 0x33, "reactive MS: Slave defect (callback)", "003807050030", "", {0, 1}},
  {ebus::MessageType::reactive, 0x33, "reactive MM: Normal", "003307040014", "", {1, 0}},
  {ebus::MessageType::reactive, 0x33, "reactive BC: Normal", "00fe0704003b", "", {1, 0}},

  {ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "33feb5050427002d002c", "feb5050427002d00", {1, 0}},
  {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost", "01feb5050427002d007b", "feb505042700cc00", {1, 0}},
  {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost/wrong byte", "01ab", "feb505042700cc00", {0, 1}},
  {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/won", "13aa33feb505042700cc0010", "feb505042700cc00", {1, 0}},
  {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/lost", "13aa13feb5050427002d0088", "feb505042700cc00", {1, 0}},
  {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority retry/error", "13a0", "feb505042700cc00", {0, 0}},
  {ebus::MessageType::active, 0x33, "active MS: Normal", "3352b509030d46003600013fa400", "52b509030d4600", {1, 0}},
  {ebus::MessageType::active, 0x33, "active MS: Master valid -> NAK -> Retry OK - Slave CRC error -> NAK -> Retry OK", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa400", "52b509030d4600", {1, 1}},
  {ebus::MessageType::active, 0x33, "active MS: Master valid -> NAK -> Retry OK - Slave CRC error -> NAK -> Retry NAK", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa3ff", "52b509030d4600", {0, 3}},
  {ebus::MessageType::active, 0x33, "active MS: Master valid -> NAK -> Retry NAK", "3352b509030d460036ff3352b509030d460036ff", "52b509030d4600", {0, 1}},
  {ebus::MessageType::active, 0x33, "active MM: Master valid -> NAK -> Retry ACK", "3310b57900fbff3310b57900fb00", "10b57900", {1, 0}},
};
// clang-format on

int main() {
  handler.setBusRequestWonCallback(busRequestWonCallback);
  handler.setBusRequestLostCallback(busRequestLostCallback);
  handler.setReactiveMasterSlaveCallback(reactiveMasterSlaveCallback);
  handler.setTelegramCallback(telegramCallback);
  handler.setErrorCallback(errorCallback);

  // Register write listener for synchronous logging
  bus.addWriteListener([](const uint8_t& byte) {
    if (g_detailed_output)
      std::cout << "<- write: " << ebus::to_string(byte) << std::endl;
  });

  for (const TestCase& tc : test_cases) {
    g_detailed_output = false;
    if (!run_test(tc)) {
      g_detailed_output = true;
      run_test(tc);
      exit(1);
    }
  }

  printMetrics();

  std::cout << "\nAll handler tests passed!" << std::endl;

  return EXIT_SUCCESS;
}
