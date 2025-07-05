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
#include "Sequence.hpp"
#include "Telegram.hpp"

#define RESET "\033[0m"
#define BOLD "\033[1m"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

void printByte(const std::string &prefix, const uint8_t &byte,
               const std::string &postfix) {
  std::cout << prefix << ebus::to_string(byte) << " " << postfix << std::endl;
}

ebus::Bus bus;
ebus::Handler handler(&bus, 0x33);

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
  ebus::Counters counter = handler.getCounters();

  // messages
  std::cout << "messagesTotal: " << counter.messagesTotal << std::endl;

  std::cout << "messagesPassiveMasterSlave: "
            << counter.messagesPassiveMasterSlave << std::endl;
  std::cout << "messagesPassiveMasterMaster: "
            << counter.messagesPassiveMasterMaster << std::endl;
  std::cout << "messagesPassiveBroadcast: " << counter.messagesPassiveBroadcast
            << std::endl;

  std::cout << "messagesReactiveMasterSlave: "
            << counter.messagesReactiveMasterSlave << std::endl;
  std::cout << "messagesReactiveMasterMaster: "
            << counter.messagesReactiveMasterMaster << std::endl;

  std::cout << "messagesActiveMasterSlave: "
            << counter.messagesActiveMasterSlave << std::endl;
  std::cout << "messagesActiveMasterMaster: "
            << counter.messagesActiveMasterMaster << std::endl;
  std::cout << "messagesActiveBroadcast: " << counter.messagesActiveBroadcast
            << std::endl
            << std::endl;

  // errors
  std::cout << "errorsTotal: " << counter.errorsTotal << std::endl << std::endl;

  std::cout << "errorsPassive: " << counter.errorsPassive << std::endl;
  std::cout << "errorsPassiveMaster: " << counter.errorsPassiveMaster
            << std::endl;
  std::cout << "errorsPassiveMasterACK: " << counter.errorsPassiveMasterACK
            << std::endl;
  std::cout << "errorsPassiveSlave: " << counter.errorsPassiveSlave
            << std::endl;
  std::cout << "errorsPassiveSlaveACK: " << counter.errorsPassiveSlaveACK
            << std::endl
            << std::endl;

  std::cout << "errorsReactive: " << counter.errorsReactive << std::endl;
  std::cout << "errorsReactiveMaster: " << counter.errorsReactiveMaster
            << std::endl;
  std::cout << "errorsReactiveMasterACK: " << counter.errorsReactiveMasterACK
            << std::endl;
  std::cout << "errorsReactiveSlave: " << counter.errorsReactiveSlave
            << std::endl;
  std::cout << "errorsReactiveSlaveACK: " << counter.errorsReactiveSlaveACK
            << std::endl
            << std::endl;

  std::cout << "errorsActive: " << counter.errorsActive << std::endl;
  std::cout << "errorsActiveMaster: " << counter.errorsActiveMaster
            << std::endl;
  std::cout << "errorsActiveMasterACK: " << counter.errorsActiveMasterACK
            << std::endl;
  std::cout << "errorsActiveSlave: " << counter.errorsActiveSlave << std::endl;
  std::cout << "errorsActiveSlaveACK: " << counter.errorsActiveSlaveACK
            << std::endl
            << std::endl;

  // resets
  std::cout << "resetsTotal: " << counter.resetsTotal << std::endl;
  std::cout << "resetsPassive00: " << counter.resetsPassive00 << std::endl;
  std::cout << "resetsPassive0704: " << counter.resetsPassive0704 << std::endl;
  std::cout << "resetsPassive: " << counter.resetsPassive << std::endl;
  std::cout << "resetsActive: " << counter.resetsActive << std::endl
            << std::endl;

  // requests
  std::cout << "requestsTotal:      " << counter.requestsTotal << std::endl;
  std::cout << "requestsWon1:       " << counter.requestsWon1 << std::endl;
  std::cout << "requestsWon2:       " << counter.requestsWon2 << std::endl;
  std::cout << "requestsLost1:      " << counter.requestsLost1 << std::endl;
  std::cout << "requestsLost2:      " << counter.requestsLost2 << std::endl;
  std::cout << "requestsError1:     " << counter.requestsError1 << std::endl;
  std::cout << "requestsError2:     " << counter.requestsError2 << std::endl;
  std::cout << "requestsErrorRetry: " << counter.requestsErrorRetry
            << std::endl;
}

void printStateTimingResults() {
  ebus::StateTimingStatsResults stats = handler.getStateTimingStatsResults();
  std::cout << std::endl << "State Timing Statistics:" << std::endl;
  for (const auto &s : stats.states) {
    std::cout << s.second.name << ": last=" << s.second.last
              << " us, mean=" << s.second.mean
              << " us, stddev=" << s.second.stddev
              << " us, count=" << s.second.count << std::endl;
  }
}

void simulate(const std::string &test, const std::string &title,
              const std::string &message, const std::string &sequence) {
  ebus::Sequence seq;
  seq.assign(ebus::to_vector(sequence));

  std::cout << "    test: " << test
            << " - address: " << ebus::to_string(handler.getAddress()) << " / "
            << ebus::to_string(handler.getSlaveAddress()) << std::endl;
  std::cout << "   title: " << RED << title << RESET << std::endl;
  if (message.size() > 0) {
    std::cout << " message: " << ebus::to_string(handler.getAddress())
              << message << std::endl;

    handler.enque(ebus::to_vector(message));
    handler.setMaxLockCounter(3);

  } else {
    std::cout << "sequence: " << sequence << std::endl;
  }
  for (size_t i = 0; i < seq.size(); i++) {
    switch (handler.getState()) {
      case ebus::FsmState::reactiveSendMasterPositiveAcknowledge:
      case ebus::FsmState::reactiveSendMasterNegativeAcknowledge:
      case ebus::FsmState::reactiveSendSlave:
      case ebus::FsmState::activeSendMaster:
      case ebus::FsmState::activeSendSlavePositiveAcknowledge:
      case ebus::FsmState::activeSendSlaveNegativeAcknowledge:
      case ebus::FsmState::releaseBus:
        i--;
        break;
      default:
        readFunction(seq[i]);
        break;
    }

    handler.run(seq[i]);

    // If SYN, simulte request bus timer
    if (seq[i] == ebus::sym_syn && handler.busRequest()) {
      std::cout << " ISR - busRequested()" << std::endl;
      bus.writeByte(handler.getAddress());
      handler.busRequested();
    }
  }

  std::cout << std::endl;
}

void passiveTest_01(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "00"
           "03b0fba901d0"  // extended a901 >> aa
           "00"
           "aaaaaa");
}

void passiveTest_02(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master defect/NAK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060044"  // defect
           "ff"                  // NAK
           "ff52b509030d060043"
           "00"
           "03b0fba901d0"
           "00"
           "aaaaaa");
}

void passiveTest_03(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/repeat");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "ff"                  // Master NAK
           "ff52b509030d060043"  // Master repeat
           "00"
           "03b0fba901d0"
           "00"
           "aaaaaa");
}

void passiveTest_04(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/repeat/NAK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "ff"                  // Master NAK
           "ff52b509030d060043"  // Master repeat
           "ff"                  // Master NAK
           "aaaaaa");
}

void passiveTest_05(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Slave defect/NAK/repeat");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "00"
           "03b0fba902d0"  // Slave defect
           "ff"            // Slave NAK
           "03b0fba901d0"  // Slave repeat
           "00"
           "aaaaaa");
}

void passiveTest_06(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Slave NAK/repeat/NAK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "00"
           "03b0fba901d0"
           "ff"            // Slave NAK
           "03b0fba901d0"  // Slave repeat
           "ff"            // Slave NAK
           "aaaaaa");
}

void passiveTest_07(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/repeat - Slave NAK/repeat");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "ff"                  // Master NAK
           "ff52b509030d060043"  // Master repeat
           "00"
           "03b0fba901d0"
           "ff"            // Slave NAK
           "03b0fba901d0"  // Slave repeat
           "00"
           "aaaaaa");
}

void passiveTest_08(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "ff52b509030d060043"
           "ff"                  // Master NAK
           "ff52b509030d060043"  // Master repeat
           "00"                  // Master ACK
           "03b0fba901d0"
           "ff"            // Slave NAK
           "03b0fba901d0"  // Slave repeat
           "ff"            // Slave NAK
           "aaaaaa");
}

void passiveTest_09(const uint8_t &address, const std::string &title) {
  assert(title == "MM: Normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "1000b5050427002400d900"
           "aaaaaa");
}

void passiveTest_10(const uint8_t &address, const std::string &title) {
  assert(title == "BC: defect");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "fe"        // broadcast
           "0704003c"  // defect
           "aaaaaa");
}

void passiveTest_11(const uint8_t &address, const std::string &title) {
  assert(title == "00: reset");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "aaaaaa");
}

void passiveTest_12(const uint8_t &address, const std::string &title) {
  assert(title == "0704: scan");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "002e0704004e"  // scan
           "aaaaaa");
}

void passiveTest_13(const uint8_t &address, const std::string &title) {
  assert(title == "BC: normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "10"
           "fe"  // broadcast
           "07000970160443183105052592"
           "aaaaaa");
}

void reactiveTest_01(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Slave NAK/ACK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "38"  // own slave address
           "070400ab"
           "ff"  // Slave NAK
           "00"  // Slave ACK
           "aaaaaa");
}

void reactiveTest_02(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Slave NAK/NAK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "38"  // own slave address
           "070400ab"
           "ff"  // Slave NAK
           "ff"  // Slave NAK
           "aaaaaa");
}

void reactiveTest_03(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master defect/correct");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "38"        // own slave address
           "070400ac"  // Master defect
           "00"
           "38"        // own slave address
           "070400ab"  // Master correct
           "00"
           "aaaaaa");
}

void reactiveTest_04(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master defect/defect");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "38"        // own slave address
           "070400ff"  // Master defect
           "00"
           "38"        // own slave address
           "070400ac"  // Master defect
           "aaaaaa");
}

void reactiveTest_05(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Slave defect (callback)");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "38"  // own slave address
           "07050030"
           "aaaaaa");
}

void reactiveTest_06(const uint8_t &address, const std::string &title) {
  assert(title == "MM: Normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "33"  // own master address
           "07040014"
           "aaaaaa");
}

void reactiveTest_07(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "",
           "aaaaaa"
           "00"
           "fe"  // broadcast
           "0704003b"
           "aaaaaa");
}

void activeTest_01(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "33"  // own Address == Arbitration won
           "aaaaaa");
}

void activeTest_02(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Priority lost");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "01"  // other Address == Priority lost
           "feb5050427002d007b"
           "aaaaaa");
}

void activeTest_03(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Priority lost/wrong byte");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "01"  // other Address == Priority lost
           "ab"  // wrong byte
           "aaaaaa");
}

void activeTest_04(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Priority fit/won");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "73"  // own Address == Priority retry
           "aa"
           "33"  // own Address == Arbitration won
           "aaaaaa");
}

void activeTest_05(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Priority fit/lost");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "73"  // own Address == Priority retry
           "aa"
           "13"  // other Address == Arbitration lost
           "aaaaaa");
}

void activeTest_06(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Priority fit/error");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "73"  // own Address == Priority retry
           "a0"  // error
           "aaaaaa");
}

void activeTest_07(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Normal");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "52b509030d4600",
           "aaaaaa"
           "33"  // own master address == Arbitration won
           "00"  // Master ACK
           "013fa4"
           "aaaaaa");
}

void activeTest_08(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/ACK - Slave CRC wrong/correct");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "52b509030d4600",
           "aaaaaa"
           "33"      // own master address == Arbitration won
           "ff"      // Master NAK
           "00"      // Master ACK
           "013fa3"  // Slave CRC wrong
           "013fa4"  // Slave CRC correct
           "aaaaaa");
}

void activeTest_09(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/ACK - Slave CRC wrong/wrong");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "52b509030d4600",
           "aaaaaa"
           "33"      // own master address == Arbitration won
           "00"      // Master ACK
           "013fa3"  // Slave CRC wrong
           "013fa3"  // Slave CRC wrong
           "aaaaaa");
}

void activeTest_10(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Master NAK/NAK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "52b509030d4600",
           "aaaaaa"
           "33"  // own master address == Arbitration won
           "ff"  // Master NAK
           "ff"  // Master NAK
           "aaaaaa");
}

void activeTest_11(const uint8_t &address, const std::string &title) {
  assert(title == "MM: Master NAK/ACK");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "10b57900",
           "aaaaaa"
           "33"  // own Address == Arbitration won
           "ff"  // Master NAK
           "00"  // Master ACK
           "aaaaaa");
}

void activeTest_12(const uint8_t &address, const std::string &title) {
  assert(title == "BC: Request Bus - Priority lost and Sub lost");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "10"                        // other Address == Priority lost
           "52b50401314b000200002c00"  //
           "aaaaaa");
}

void activeTest_13(const uint8_t &address, const std::string &title) {
  assert(title == "MS: Request Bus - Priority lost to 0x10");
  handler.setAddress(address);
  simulate(__FUNCTION__, title, "feb5050427002d00",
           "aaaaaa"
           "10"                        // other Address == Priority lost
           "52b50401314b000200002c00"  //
           "aaaaaa");
}

int main() {
  handler.setReactiveMasterSlaveCallback(reactiveMasterSlaveCallback);
  handler.setTelegramCallback(telegramCallback);
  handler.setErrorCallback(onErrorCallback);

  // clang-format off
  passiveTest_01(0x33,  "MS: Normal");
  passiveTest_02(0x33,  "MS: Master defect/NAK");
  passiveTest_03(0x33,  "MS: Master NAK/repeat");
  passiveTest_04(0x33,  "MS: Master NAK/repeat/NAK");
  passiveTest_05(0x33,  "MS: Slave defect/NAK/repeat");
  passiveTest_06(0x33,  "MS: Slave NAK/repeat/NAK");
  passiveTest_07(0x33,  "MS: Master NAK/repeat - Slave NAK/repeat");
  passiveTest_08(0x33,  "MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK");
  passiveTest_09(0x33,  "MM: Normal");
  passiveTest_10(0x33,  "BC: defect");
  passiveTest_11(0x33,  "00: reset");
  passiveTest_12(0x33,  "0704: scan");
  passiveTest_13(0x33,  "BC: normal");

  reactiveTest_01(0x33, "MS: Slave NAK/ACK");
  reactiveTest_02(0x33, "MS: Slave NAK/NAK");
  reactiveTest_03(0x33, "MS: Master defect/correct");
  reactiveTest_04(0x33, "MS: Master defect/defect");
  reactiveTest_05(0x33, "MS: Slave defect (callback)");
  reactiveTest_06(0x33, "MM: Normal");
  reactiveTest_07(0x33, "BC: Normal");

  activeTest_01(0x33, "BC: Request Bus - Normal");
  activeTest_02(0x33, "BC: Request Bus - Priority lost");
  activeTest_03(0x33, "BC: Request Bus - Priority lost/wrong byte");
  activeTest_04(0x33, "BC: Request Bus - Priority fit/won");
  activeTest_05(0x33, "BC: Request Bus - Priority fit/lost");
  activeTest_06(0x33, "BC: Request Bus - Priority fit/error");
  activeTest_07(0x33, "MS: Normal");
  activeTest_08(0x33, "MS: Master NAK/ACK - Slave CRC wrong/correct");
  activeTest_09(0x33, "MS: Master NAK/ACK - Slave CRC wrong/wrong");
  activeTest_10(0x33, "MS: Master NAK/NAK");
  activeTest_11(0x33, "MM: Master NAK/ACK");
  activeTest_12(0x30, "BC: Request Bus - Priority lost and Sub lost");
  activeTest_13(0x30, "MS: Request Bus - Priority lost to 0x10");
  // clang-format on

  printCounters();

  // printStateTimingResults();

  return EXIT_SUCCESS;
}
