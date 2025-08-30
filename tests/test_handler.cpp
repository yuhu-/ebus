/*
 * Copyright (C) 2023-2025 Roland Jax
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
  bool enabled;
  ebus::MessageType messageType;
  uint8_t address;
  std::string description;
  std::string read_string;
  std::string send_string = "";
};

void printByte(const std::string &prefix, const uint8_t &byte,
               const std::string &postfix) {
  std::cout << prefix << ebus::to_string(byte) << " " << postfix << std::endl;
}

ebus::Bus bus;
ebus::Request request;
ebus::Handler handler(ebus::DEFAULT_ADDRESS, &bus, &request);

void readFunction(const uint8_t &byte) {
  std::cout << "->  read: " << ebus::to_string(byte) << std::endl;
}

void reactiveMasterSlaveCallback(const std::vector<uint8_t> &master,
                                 std::vector<uint8_t> *const slave) {
  std::vector<uint8_t> search;
  search = {0x07, 0x04};  // 0008070400
  if (ebus::contains(master, search))
    *slave = ebus::to_vector("0ab5504d53303001074302");
  search = {0x07, 0x05};  // 0008070500
  if (ebus::contains(master, search))
    *slave = ebus::to_vector("0ab5504d533030010743");  // defect

  std::cout << "callback: " << ebus::to_string(master) << " "
            << ebus::to_string(*slave) << std::endl;
}

void telegramCallback(const ebus::MessageType &messageType,
                      const ebus::TelegramType &telegramType,
                      const std::vector<uint8_t> &master,
                      const std::vector<uint8_t> &slave) {
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

void onErrorCallback(const std::string &error,
                     const std::vector<uint8_t> &master,
                     const std::vector<uint8_t> &slave) {
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
  std::cout << "resetActive: " << handlerCounter.resetActive << std::endl
            << std::endl;

  ebus::Request::Counter requestCounter = request.getCounter();

  // requests
  std::cout << "requestsStartBit:    " << requestCounter.requestsStartBit
            << std::endl;
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

void run_test(const TestCase &tc) {
  std::cout << std::endl
            << "=== Test: " << tc.description << " ===" << std::endl;

  handler.setSourceAddress(tc.address);

  // Prepare test sequence from the provided hex string
  std::string tmp = "aaaaaa" + tc.read_string + "aaaaaa";
  ebus::Sequence seq;
  seq.assign(ebus::to_vector(tmp));

  if (tc.send_string.size() > 0)
    handler.enqueueActiveMessage(ebus::to_vector(tc.send_string));

  for (size_t i = 0; i < seq.size(); i++) {
    switch (handler.getState()) {
      case ebus::HandlerState::reactiveSendMasterPositiveAcknowledge:
      case ebus::HandlerState::reactiveSendMasterNegativeAcknowledge:
      case ebus::HandlerState::reactiveSendSlave:
      case ebus::HandlerState::activeSendMaster:
      case ebus::HandlerState::activeSendSlavePositiveAcknowledge:
      case ebus::HandlerState::activeSendSlaveNegativeAcknowledge:
      case ebus::HandlerState::releaseBus:
        i--;
        break;
      default:
        readFunction(seq[i]);
        break;
    }

    request.run(seq[i]);
    handler.run(seq[i]);

    // simulte request bus timer
    if (seq[i] == ebus::sym_syn && request.busRequestPending()) {
      std::cout << " ISR - write address" << std::endl;
      bus.writeByte(request.getAddress());
      request.busRequestCompleted();
    }
  }

  request.reset();
  handler.reset();

  std::cout << "--- Test: " << tc.description << " ---" << std::endl;
}

// clang-format off
std::vector<TestCase> test_cases = {
  {false, ebus::MessageType::passive, 0x33, "passive MS: Normal", "ff52b509030d0600430003b0fba901d000"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Master defect/NAK", "ff52b509030d060044ffff52b509030d0600430003b0fba901d000"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat", "ff52b509030d060043ffff52b509030d0600430003b0fba901d000"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat/NAK", "ff52b509030d060043ffff52b509030d060043ff"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Slave defect/NAK/repeat", "ff52b509030d0600430003b0fba902d0ff03b0fba901d000"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Slave NAK/repeat/NAK", "ff52b509030d0600430003b0fba901d0ff03b0fba901d0ff"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat - Slave NAK/repeat", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d000"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d0ff"},
  {false, ebus::MessageType::passive, 0x33, "passive MM: Normal", "1000b5050427002400d900"},
  {false, ebus::MessageType::passive, 0x33, "passive BC: defect", "00fe0704003c"},
  {false, ebus::MessageType::passive, 0x33, "passive 00: reset", "00"},
  {false, ebus::MessageType::passive, 0x33, "passive 0704: scan", "002e0704004e"},
  {false, ebus::MessageType::passive, 0x33, "passive BC: normal", "10fe07000970160443183105052592"},
  {false, ebus::MessageType::passive, 0x33, "passive MS: slave CRC byte is invalid", "1008b5130304cd017f000acd01000000000100010000"},

  // {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/ACK", "0038070400ab000ab5504d5330300107430246ff0ab5504d533030010743024600"},
  {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/ACK", "0038070400abff00"},
  // {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/NAK", "0038070400ab000ab5504d5330300107430246ff0ab5504d5330300107430246ff"},
  {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/NAK", "0038070400abffff"},
  // {false, ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/correct", "0038070400acff0038070400ab000ab5504d533030010743024600"},
  {false, ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/correct", "0038070400ac0038070400ab00"},
  // {false, ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/defect", "0038070400acff0038070400acff"},
  {false, ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/defect", "0038070400ff0038070400ac"},
  // {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave defect (callback)", "003807050030aa"},
  {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave defect (callback)", "003807050030"},
  {false, ebus::MessageType::reactive, 0x33, "reactive MM: Normal", "003307040014"},
  {false, ebus::MessageType::reactive, 0x33, "reactive BC: Normal", "00fe0704003b"},

  // {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "33feb5050427002d00", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "33", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost", "01feb5050427002d007b", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost/wrong byte", "01ab", "feb5050427002d00"},
  // {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/won", "73aa33feb5050427002d00", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/won", "73aa33", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/lost", "73aa13", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority retry/error", "73a0", "feb5050427002d00"},
  // {false, ebus::MessageType::active, 0x33, "active MS: Normal", "3352b509030d46003600013fa4", "52b509030d4600"},
  {false, ebus::MessageType::active, 0x33, "active MS: Normal", "3300013fa4", "52b509030d4600"},
// {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/ACK - Slave CRC wrong/correct", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa4", "52b509030d4600"},
  {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/ACK - Slave CRC wrong/correct", "33ff00013fa3013fa4", "52b509030d4600"},
  // {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/ACK - Slave CRC wrong/wrong", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa3ff", "52b509030d4600"},
  {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/ACK - Slave CRC wrong/wrong", "3300013fa3013fa3", "52b509030d4600"},
  // {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/NAK", "3352b509030d460036ff3352b509030d460036ff", "52b509030d4600"},
  {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/NAK", "33ffff", "52b509030d4600"},
  // {false, ebus::MessageType::active, 0x33, "active MM: Master NAK/ACK", "3310b57900fbff3310b57900fb00", "10b57900"},
  {false, ebus::MessageType::active, 0x33, "active MM: Master NAK/ACK", "33ff00", "10b57900"},
  {false, ebus::MessageType::active, 0x30, "active BC: Request Bus - Priority lost and Sub lost", "1052b50401314b000200002c00", "feb5050427002d00"},
  {false, ebus::MessageType::active, 0x30, "active MS: Request Bus - Priority lost to 0x10", "1052b50401314b000200002c00","feb5050427002d00"}
};
// clang-format on

void enable_group(const ebus::MessageType &messageType) {
  for (TestCase &tc : test_cases)
    if (tc.messageType == messageType) tc.enabled = true;
}

int main() {
  handler.setReactiveMasterSlaveCallback(reactiveMasterSlaveCallback);
  handler.setTelegramCallback(telegramCallback);
  handler.setErrorCallback(onErrorCallback);

  enable_group(ebus::MessageType::passive);
  enable_group(ebus::MessageType::reactive);
  enable_group(ebus::MessageType::active);

  for (const TestCase &tc : test_cases)
    if (tc.enabled) run_test(tc);

  printCounters();

  return EXIT_SUCCESS;
}
