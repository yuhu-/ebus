/*
 * Copyright (C) 2023-2026 Roland Jax
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

#include <cassert>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "Bus.hpp"
#include "Common.hpp"
#include "Datatypes.hpp"
#include "Handler.hpp"
#include "Request.hpp"
#include "Sequence.hpp"
#include "Telegram.hpp"

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
ebus::Bus bus(config, &request);

ebus::Handler handler(ebus::DEFAULT_ADDRESS, &bus, &request);

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

void printCounters() {
  ebus::Handler::Counter handlerCounter = handler.getCounter();

  // messages
  std::cout << std::endl
            << "messagesTotal: " << handlerCounter.messagesTotal << std::endl;

  std::cout << "messagesPassiveMasterSlave: "
            << handlerCounter.messagesPassiveMasterSlave << std::endl;
  std::cout << "messagesPassiveMasterMaster: "
            << handlerCounter.messagesPassiveMasterMaster << std::endl;
  std::cout << "messagesPassiveBroadcast: "
            << handlerCounter.messagesPassiveBroadcast << std::endl;

  std::cout << "messagesReactiveMasterSlave: "
            << handlerCounter.messagesReactiveMasterSlave << std::endl;
  std::cout << "messagesReactiveMasterMaster: "
            << handlerCounter.messagesReactiveMasterMaster << std::endl;

  std::cout << "messagesActiveMasterSlave: "
            << handlerCounter.messagesActiveMasterSlave << std::endl;
  std::cout << "messagesActiveMasterMaster: "
            << handlerCounter.messagesActiveMasterMaster << std::endl;
  std::cout << "messagesActiveBroadcast: "
            << handlerCounter.messagesActiveBroadcast << std::endl
            << std::endl;

  // error
  std::cout << "errorTotal: " << handlerCounter.errorTotal << std::endl
            << std::endl;

  std::cout << "errorPassive: " << handlerCounter.errorPassive << std::endl;
  std::cout << "errorPassiveMaster: " << handlerCounter.errorPassiveMaster
            << std::endl;
  std::cout << "errorPassiveMasterACK: " << handlerCounter.errorPassiveMasterACK
            << std::endl;
  std::cout << "errorPassiveSlave: " << handlerCounter.errorPassiveSlave
            << std::endl;
  std::cout << "errorPassiveSlaveACK: " << handlerCounter.errorPassiveSlaveACK
            << std::endl
            << std::endl;

  std::cout << "errorReactive: " << handlerCounter.errorReactive << std::endl;
  std::cout << "errorReactiveMaster: " << handlerCounter.errorReactiveMaster
            << std::endl;
  std::cout << "errorReactiveMasterACK: "
            << handlerCounter.errorReactiveMasterACK << std::endl;
  std::cout << "errorReactiveSlave: " << handlerCounter.errorReactiveSlave
            << std::endl;
  std::cout << "errorReactiveSlaveACK: " << handlerCounter.errorReactiveSlaveACK
            << std::endl
            << std::endl;

  std::cout << "errorActive: " << handlerCounter.errorActive << std::endl;
  std::cout << "errorActiveMaster: " << handlerCounter.errorActiveMaster
            << std::endl;
  std::cout << "errorActiveMasterACK: " << handlerCounter.errorActiveMasterACK
            << std::endl;
  std::cout << "errorActiveSlave: " << handlerCounter.errorActiveSlave
            << std::endl;
  std::cout << "errorActiveSlaveACK: " << handlerCounter.errorActiveSlaveACK
            << std::endl
            << std::endl;

  // reset
  std::cout << "resetTotal: " << handlerCounter.resetTotal << std::endl;
  std::cout << "resetPassive00: " << handlerCounter.resetPassive00 << std::endl;
  std::cout << "resetPassive0704: " << handlerCounter.resetPassive0704
            << std::endl;
  std::cout << "resetPassive: " << handlerCounter.resetPassive << std::endl;
  std::cout << "resetActive00: " << handlerCounter.resetActive00 << std::endl;
  std::cout << "resetActive0704: " << handlerCounter.resetActive0704
            << std::endl;
  std::cout << "resetActive: " << handlerCounter.resetActive << std::endl
            << std::endl;

  ebus::Request::Counter requestCounter = request.getCounter();

  // requests
  std::cout << "requestsFirstSyn:    " << requestCounter.requestsFirstSyn
            << std::endl;
  std::cout << "requestsFirstWon:    " << requestCounter.requestsFirstWon
            << std::endl;
  std::cout << "requestsFirstRetry:  " << requestCounter.requestsFirstRetry
            << std::endl;
  std::cout << "requestsFirstLost:   " << requestCounter.requestsFirstLost
            << std::endl;
  std::cout << "requestsFirstError:  " << requestCounter.requestsFirstError
            << std::endl;
  std::cout << "requestsRetrySyn:    " << requestCounter.requestsRetrySyn
            << std::endl;
  std::cout << "requestsRetryError:  " << requestCounter.requestsRetryError
            << std::endl;
  std::cout << "requestsSecondWon:   " << requestCounter.requestsSecondWon
            << std::endl;
  std::cout << "requestsSecondLost:  " << requestCounter.requestsSecondLost
            << std::endl;
  std::cout << "requestsSecondError: " << requestCounter.requestsSecondError
            << std::endl;
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

  // Remember last written bytes to detect new ones
  size_t written_count = bus.getWrittenByteCount();

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
    handler.run(seq[i]);

    // Check if anything was written and log it
    if (bus.getWrittenByteCount() > written_count) {
      if (g_detailed_output)
        std::cout << "<- write: " << ebus::to_string(bus.getLastWrittenByte())
                  << std::endl;
      written_count = bus.getWrittenByteCount();
    }

    // simulte request bus timer
    if (seq[i] == ebus::sym_syn && request.busRequestPending()) {
      if (g_detailed_output) std::cout << " ISR - write address" << std::endl;
      bus.writeByte(request.busRequestAddress());
      busRequestFlag = true;

      if (bus.getWrittenByteCount() > written_count) {
        if (g_detailed_output)
          std::cout << "<- write: " << ebus::to_string(bus.getLastWrittenByte())
                    << std::endl;
        written_count = bus.getWrittenByteCount();
      }
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

  for (const TestCase& tc : test_cases) {
    g_detailed_output = false;
    if (!run_test(tc)) {
      g_detailed_output = true;
      run_test(tc);
      exit(1);
    }
  }

  // printCounters();

  std::cout << "\nAll handler tests passed!" << std::endl;

  return EXIT_SUCCESS;
}
