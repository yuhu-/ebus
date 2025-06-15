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

bool printBytes = false;

ebus::Handler ebusHandler(0x33);

const char *getState() { return ebus::getFsmStateText(ebusHandler.getState()); }

void printByte(const std::string &prefix, const uint8_t &byte,
               const std::string &postfix) {
  std::cout << prefix << ebus::to_string(byte) << " " << postfix << std::endl;
}

void readFunction(const uint8_t &byte) {
  if (printBytes) printByte("->  read: ", byte, getState());
}

void onWriteCallback(const uint8_t &byte) {
  if (printBytes) printByte("<- write: ", byte, getState());
}

int isDataAvailableCallback() { return 0; }

void onTelegramCallback(const ebus::MessageType &message,
                        const ebus::TelegramType &type,
                        const std::vector<uint8_t> &master,
                        std::vector<uint8_t> *const slave) {
  std::vector<uint8_t> search;
  std::string typeString = "";
  switch (message) {
    case ebus::MessageType::active:
      std::cout << "  active: " << ebus::to_string(master) << " "
                << ebus::to_string(*slave) << std::endl;
      break;
    case ebus::MessageType::passive:
      std::cout << " passive: " << ebus::to_string(master) << " "
                << ebus::to_string(*slave) << std::endl;
      break;
    case ebus::MessageType::reactive:

      switch (type) {
        case ebus::TelegramType::broadcast:
          typeString = "broadcast message";
          break;
        case ebus::TelegramType::master_master:
          typeString = "master master message";
          break;
        case ebus::TelegramType::master_slave:
          typeString = "master slave message";
          search = {0x07, 0x04};  // 0008070400
          if (ebus::contains(master, search))
            *slave = ebus::to_vector("0ab5504d53303001074302");
          search = {0x07, 0x05};  // 0008070500
          if (ebus::contains(master, search))
            *slave = ebus::to_vector("0ab5504d533030010743");  // defect
          break;
        default:
          break;
      }
      std::cout << "    type: " << typeString << std::endl;
      std::cout << "reactive: " << ebus::to_string(master) << " "
                << ebus::to_string(*slave) << std::endl;

      break;
    default:
      break;
  }
}

void onErrorCallback(const std::string &str) {
  std::cout << "   error: " << str << std::endl;
}

void printCounters() {
  ebus::Counters counter = ebusHandler.getCounters();

  // messages
  std::cout << "messagesTotal: " << counter.messagesTotal << std::endl;

  std::cout << "messagesPassiveMasterSlave: "
            << counter.messagesPassiveMasterSlave << std::endl;
  std::cout << "messagesPassiveMasterMaster: "
            << counter.messagesPassiveMasterMaster << std::endl;

  std::cout << "messagesReactiveMasterSlave: "
            << counter.messagesReactiveMasterSlave << std::endl;
  std::cout << "messagesReactiveMasterMaster: "
            << counter.messagesReactiveMasterMaster << std::endl;
  std::cout << "messagesReactiveBroadcast: "
            << counter.messagesReactiveBroadcast << std::endl;

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
  std::cout << "requestsTotal: " << counter.requestsTotal << std::endl;
  std::cout << "requestsWon: " << counter.requestsWon << std::endl;
  std::cout << "requestsLost: " << counter.requestsLost << std::endl;
  std::cout << "requestsRetry: " << counter.requestsRetry << std::endl;
  std::cout << "requestsError: " << counter.requestsError << std::endl;
}

void printStateTimingResults() {
  ebus::StateTimingStatsResults stats =
      ebusHandler.getStateTimingStatsResults();
  std::cout << std::endl << "State Timing Statistics:" << std::endl;
  for (const auto &s : stats.states) {
    std::cout << s.second.name << ": last=" << s.second.last
              << " us, mean=" << s.second.mean
              << " us, stddev=" << s.second.stddev
              << " us, count=" << s.second.count << std::endl;
  }
}

void simulate(const std::string &test, const std::string &title,
              const bool &bytes, const std::string &message,
              const std::string &sequence) {
  printBytes = bytes;
  ebus::Sequence seq;
  seq.assign(ebus::to_vector(sequence));

  std::cout << "    test: " << test
            << " - address: " << ebus::to_string(ebusHandler.getAddress())
            << " / " << ebus::to_string(ebusHandler.getSlaveAddress())
            << std::endl;
  std::cout << "   title: " << RED << title << RESET << std::endl;
  if (message.size() > 0) {
    std::cout << " message: " << ebus::to_string(ebusHandler.getAddress())
              << message << std::endl;

    ebusHandler.enque(ebus::to_vector(message));
    ebusHandler.setMaxLockCounter(3);

  } else {
    std::cout << "sequence: " << sequence << std::endl;
  }
  for (size_t i = 0; i < seq.size(); i++) {
    switch (ebusHandler.getState()) {
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

    ebusHandler.run(seq[i]);
  }

  std::cout << std::endl;
}

void passiveTest_01(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "ff52b509030d060043"
           "00"
           "03b0fba901d0"  // extended a901 >> aa
           "00"
           "aaaaaa");
}

void passiveTest_02(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Master defect/NAK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "ff52b509030d060044"  // defect
           "ff"                  // NAK
           "ff52b509030d060043"
           "00"
           "03b0fba901d0"
           "00"
           "aaaaaa");
}

void passiveTest_03(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Master NAK/repeat");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "ff52b509030d060043"
           "ff"                  // Master NAK
           "ff52b509030d060043"  // Master repeat
           "00"
           "03b0fba901d0"
           "00"
           "aaaaaa");
}

void passiveTest_04(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Master NAK/repeat/NAK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "ff52b509030d060043"
           "ff"                  // Master NAK
           "ff52b509030d060043"  // Master repeat
           "ff"                  // Master NAK
           "aaaaaa");
}

void passiveTest_05(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Slave defect/NAK/repeat");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "ff52b509030d060043"
           "00"
           "03b0fba902d0"  // Slave defect
           "ff"            // Slave NAK
           "03b0fba901d0"  // Slave repeat
           "00"
           "aaaaaa");
}

void passiveTest_06(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Slave NAK/repeat/NAK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "ff52b509030d060043"
           "00"
           "03b0fba901d0"
           "ff"            // Slave NAK
           "03b0fba901d0"  // Slave repeat
           "ff"            // Slave NAK
           "aaaaaa");
}

void passiveTest_07(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Master NAK/repeat - Slave NAK/repeat");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
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

void passiveTest_08(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
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

void passiveTest_09(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "MM: Normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "1000b5050427002400d900"
           "aaaaaa");
}

void passiveTest_10(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "BC: defect");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "fe"        // broadcast
           "0704003c"  // defect
           "aaaaaa");
}

void passiveTest_11(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "00: reset");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "aaaaaa");
}

void passiveTest_12(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "0704: scan");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "002e0704004e"  // scan
           "aaaaaa");
}

void passiveTest_13(const uint8_t &address, const bool &bytes,
                    const std::string &title) {
  assert(title == "BC: normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "10"
           "fe"  // broadcast
           "07000970160443183105052592"
           "aaaaaa");
}

void reactiveTest_01(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "MS: Slave NAK/ACK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "38"  // own slave address
           "070400ab"
           "ff"  // Slave NAK
           "00"  // Slave ACK
           "aaaaaa");
}

void reactiveTest_02(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "MS: Slave NAK/NAK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "38"  // own slave address
           "070400ab"
           "ff"  // Slave NAK
           "ff"  // Slave NAK
           "aaaaaa");
}

void reactiveTest_03(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "MS: Master defect/correct");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "38"        // own slave address
           "070400ff"  // Master defect
           "00"
           "38"        // own slave address
           "070400ab"  // Master correct
           "00"
           "aaaaaa");
}

void reactiveTest_04(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "MS: Master defect/defect");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "38"        // own slave address
           "070400ff"  // Master defect
           "00"
           "38"        // own slave address
           "070400ac"  // Master defect
           "aaaaaa");
}

void reactiveTest_05(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "MS: Slave defect (reactiveCallback)");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "38"  // own slave address
           "07050030"
           "aaaaaa");
}

void reactiveTest_06(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "MM: Normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "33"  // own master address
           "07040014"
           "aaaaaa");
}

void reactiveTest_07(const uint8_t &address, const bool &bytes,
                     const std::string &title) {
  assert(title == "BC: Normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "",
           "aaaaaa"
           "00"
           "fe"  // broadcast
           "0704003b"
           "aaaaaa");
}

void activeTest_01(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "33"  // own Address == Arbitration won
           "aaaaaa");
}

void activeTest_02(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Priority lost");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "01"  // other Address == Priority lost
           "feb5050427002d007b"
           "aaaaaa");
}

void activeTest_03(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Priority lost/wrong byte");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "01"  // other Address == Priority lost
           "ab"  // wrong byte
           "aaaaaa");
}

void activeTest_04(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Priority fit/won");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "83"  // own Address == Priority retry
           "aa"
           "33"  // own Address == Arbitration won
           "aaaaaa");
}

void activeTest_05(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Priority fit/lost");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "83"  // own Address == Priority retry
           "aa"
           "13"  // other Address == Arbitration lost
           "aaaaaa");
}

void activeTest_06(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Priority retry/error");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "73"  // own Address == Priority retry
           "a0"  // error
           "aaaaaa");
}

void activeTest_07(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "MS: Normal");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "52b509030d4600",
           "aaaaaa"
           "33"  // own master address == Arbitration won
           "00"  // Master ACK
           "013fa4"
           "aaaaaa");
}

void activeTest_08(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "MS: Master NAK/ACK - Slave CRC wrong/correct");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "52b509030d4600",
           "aaaaaa"
           "33"      // own master address == Arbitration won
           "ff"      // Master NAK
           "00"      // Master ACK
           "013fa3"  // Slave CRC wrong
           "013fa4"  // Slave CRC correct
           "aaaaaa");
}

void activeTest_09(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "MS: Master NAK/ACK - Slave CRC wrong/wrong");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "52b509030d4600",
           "aaaaaa"
           "33"      // own master address == Arbitration won
           "00"      // Master ACK
           "013fa3"  // Slave CRC wrong
           "013fa3"  // Slave CRC wrong
           "aaaaaa");
}

void activeTest_10(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "MS: Master NAK/NAK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "52b509030d4600",
           "aaaaaa"
           "33"  // own master address == Arbitration won
           "ff"  // Master NAK
           "ff"  // Master NAK
           "aaaaaa");
}

void activeTest_11(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "MM: Master NAK/ACK");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "10b57900",
           "aaaaaa"
           "33"  // own Address == Arbitration won
           "ff"  // Master NAK
           "00"  // Master ACK
           "aaaaaa");
}

void activeTest_12(const uint8_t &address, const bool &bytes,
                   const std::string &title) {
  assert(title == "BC: Request Bus - Priority lost and Sub lost");
  ebusHandler.setAddress(address);
  simulate(__FUNCTION__, title, bytes, "feb5050427002d00",
           "aaaaaa"
           "10"                        // other Address == Priority lost
           "52b50401314b000200002c00"  //
           "aaaaaa");
}

int main() {
  ebusHandler.onWrite(onWriteCallback);
  ebusHandler.isDataAvailable(isDataAvailableCallback);
  ebusHandler.onTelegram(onTelegramCallback);
  ebusHandler.onError(onErrorCallback);

  passiveTest_01(0x33, true, "MS: Normal");
  passiveTest_02(0x33, true, "MS: Master defect/NAK");
  passiveTest_03(0x33, true, "MS: Master NAK/repeat");
  passiveTest_04(0x33, true, "MS: Master NAK/repeat/NAK");
  passiveTest_05(0x33, true, "MS: Slave defect/NAK/repeat");
  passiveTest_06(0x33, true, "MS: Slave NAK/repeat/NAK");
  passiveTest_07(0x33, true, "MS: Master NAK/repeat - Slave NAK/repeat");
  passiveTest_08(0x33, true,
                 "MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK");
  passiveTest_09(0x33, true, "MM: Normal");
  passiveTest_10(0x33, true, "BC: defect");
  passiveTest_11(0x33, true, "00: reset");
  passiveTest_12(0x33, true, "0704: scan");
  passiveTest_13(0x33, true, "BC: normal");

  reactiveTest_01(0x33, true, "MS: Slave NAK/ACK");
  reactiveTest_02(0x33, true, "MS: Slave NAK/NAK");
  reactiveTest_03(0x33, true, "MS: Master defect/correct");
  reactiveTest_04(0x33, true, "MS: Master defect/defect");
  reactiveTest_05(0x33, true, "MS: Slave defect (reactiveCallback)");
  reactiveTest_06(0x33, true, "MM: Normal");
  reactiveTest_07(0x33, true, "BC: Normal");

  activeTest_01(0x33, true, "BC: Request Bus - Normal");
  activeTest_02(0x33, true, "BC: Request Bus - Priority lost");
  activeTest_03(0x33, true, "BC: Request Bus - Priority lost/wrong byte");
  activeTest_04(0x33, true, "BC: Request Bus - Priority fit/won");
  activeTest_05(0x33, true, "BC: Request Bus - Priority fit/lost");
  activeTest_06(0x33, true, "BC: Request Bus - Priority retry/error");
  activeTest_07(0x33, true, "MS: Normal");
  activeTest_08(0x33, true, "MS: Master NAK/ACK - Slave CRC wrong/correct");
  activeTest_09(0x33, true, "MS: Master NAK/ACK - Slave CRC wrong/wrong");
  activeTest_10(0x33, true, "MS: Master NAK/NAK");
  activeTest_11(0x33, true, "MM: Master NAK/ACK");
  activeTest_12(0x30, true, "BC: Request Bus - Priority lost and Sub lost");

  printCounters();

  printStateTimingResults();

  return EXIT_SUCCESS;
}
