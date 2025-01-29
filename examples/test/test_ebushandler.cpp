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

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "Datatypes.h"
#include "EbusHandler.h"
#include "Sequence.h"
#include "Telegram.h"

bool printBytes = false;

// declaration
const char *getState();

// definition
const std::string to_string(const uint8_t &byte) {
  return ebus::Sequence::to_string(std::vector<uint8_t>(1, byte));
}

void printByte(const std::string prefix, const uint8_t &byte,
               const std::string postfix) {
  std::cout << prefix << to_string(byte) << " " << postfix << std::endl;
}

void busReadFunction(const uint8_t &byte) {
  if (printBytes) printByte("->  read: ", byte, getState());
}

bool busReadyCallback() { return true; }

void busWriteCallback(const uint8_t &byte) {
  if (printBytes) printByte("<- write: ", byte, getState());
}

void activeCallback(const std::vector<uint8_t> &master,
                    const std::vector<uint8_t> &slave) {
  std::cout << "  active: " << ebus::Sequence::to_string(master) << " "
            << ebus::Sequence::to_string(slave) << std::endl;
}

void passiveCallback(const std::vector<uint8_t> &master,
                     const std::vector<uint8_t> &slave) {
  std::cout << " passive: " << ebus::Sequence::to_string(master) << " "
            << ebus::Sequence::to_string(slave) << std::endl;
}

void reactiveCallback(const std::vector<uint8_t> &master,
                      std::vector<uint8_t> *const slave) {
  std::vector<uint8_t> search;
  std::string typeString = "";
  switch (ebus::Telegram::typeOf(master[1])) {
    case ebus::Type::BC:
      typeString = "broadcast message";
      break;
    case ebus::Type::MM:
      typeString = "master master message";
      break;
    case ebus::Type::MS:
      typeString = "master slave message";
      search = {0x07, 0x04};  // 0008070400
      if (ebus::Sequence::contains(master, search))
        *slave = ebus::Sequence::to_vector("0ab5504d53303001074302");
      search = {0x07, 0x05};  // 0008070500
      if (ebus::Sequence::contains(master, search))
        *slave = ebus::Sequence::to_vector("0ab5504d533030010743");
      break;
    default:
      break;
  }

  std::cout << "    type: " << typeString << std::endl;
  std::cout << "reactive: " << ebus::Sequence::to_string(master) << " "
            << (slave != nullptr ? ebus::Sequence::to_string(*slave) : "")
            << std::endl;
}

ebus::EbusHandler ebusHandler(0x33, &busReadyCallback, &busWriteCallback,
                              &activeCallback, &passiveCallback,
                              &reactiveCallback);

const char *getState() { return ebus::stateString(ebusHandler.getState()); }

void testCallback(const bool &external, const std::string &test,
                  const std::string &header, const std::string &message,
                  const std::string &sequence) {
  bool locked = false;
  ebus::Sequence seq;
  seq.assign(ebus::Sequence::to_vector(sequence));

  std::cout << "    test: " << test << std::endl;
  std::cout << " address: " << to_string(ebusHandler.getAddress()) << " ("
            << to_string(ebusHandler.getSlaveAddress()) << ")" << std::endl;
  std::cout << "    name: " << header << std::endl;
  if (message.size() > 0) {
    std::cout << " message: " << to_string(ebusHandler.getAddress()) << message
              << std::endl;

    ebusHandler.enque(ebus::Sequence::to_vector(message));
    ebusHandler.setMaxLockCounter(3);
    ebusHandler.setExternalBusRequest(external);

  } else {
    std::cout << "sequence: " << sequence << std::endl;
  }
  for (size_t i = 0; i < seq.size(); i++) {
    switch (ebusHandler.getState()) {
      case ebus::State::reactiveSendMasterPositiveAcknowledge:
      case ebus::State::reactiveSendMasterNegativeAcknowledge:
      case ebus::State::reactiveSendSlave:
      case ebus::State::activeSendMaster:
      case ebus::State::activeSendSlavePositiveAcknowledge:
      case ebus::State::activeSendSlaveNegativeAcknowledge:
      case ebus::State::releaseBus:
        i--;
        break;
      default:
        busReadFunction(seq[i]);
        break;
    }

    if (external && !locked) {
      if (seq[i] == ebusHandler.getAddress()) {
        ebusHandler.stateExternalBusRequest(true);
        locked = true;
      }
    }

    ebusHandler.run(seq[i]);
  }

  std::cout << std::endl;
}

void testPassive() {
  std::string test = "passiveCallback";
  std::string header;

  header = "MS: Normal";
  testCallback(false, test, header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"  // extended a901 >> aa
               "00"
               "aaaaaa");

  header = "MS: Master defect/NAK";
  testCallback(false, test, header, "",
               "aaaaaa"
               "ff52b509030d060044"  // defect
               "ff"                  // NAK
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"
               "00"
               "aaaaaa");

  header = "MS: Master NAK/repeat";
  testCallback(false, test, header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "ff"                  // Master NAK
               "ff52b509030d060043"  // Master repeat
               "00"
               "03b0fba901d0"
               "00"
               "aaaaaa");

  header = "MS: Master NAK/repeat/NAK";
  testCallback(false, test, header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "ff"                  // Master NAK
               "ff52b509030d060043"  // Master repeat
               "ff"                  // Master NAK
               "aaaaaa");

  header = "MS: Slave NAK/repeat";
  testCallback(false, test, header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"
               "ff"            // Slave NAK
               "03b0fba901d0"  // Slave repeat
               "00"
               "aaaaaa");

  header = "MS: Slave NAK/repeat/NAK";
  testCallback(false, test, header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"
               "ff"            // Slave NAK
               "03b0fba901d0"  // Slave repeat
               "ff"            // Slave NAK
               "aaaaaa");

  header = "MS: Master NAK/repeat - Slave NAK/repeat";
  testCallback(false, test, header, "",
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

  header = "MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK";
  testCallback(false, test, header, "",
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

  header = "MM: Normal";
  testCallback(false, test, header, "",
               "aaaaaa"
               "1000b5050427002400d900"
               "aaaaaa");

  header = "BC: defect";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "fe"        // broadcast
               "0704003c"  // defect
               "aaaaaa");

  header = "00: reset";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "aaaaaa");
}

void testReactive() {
  std::string test = "reactiveCallback";
  std::string header;

  header = "MS: Slave NAK/ACK";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "38"  // own slave address
               "070400ab"
               "ff"  // Slave NAK
               "00"  // Slave ACK
               "aaaaaa");

  header = "MS: Slave defect";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "38"        // own slave address
               "07050031"  // Slave defect
               "aaaaaa");

  header = "MS: Master defect/correct";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "38"        // own slave address
               "070400ff"  // Master defect
               "00"
               "38"        // own slave address
               "070400ab"  // Master correct
               "00"
               "aaaaaa");

  header = "MM: Normal";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "33"  // own master address
               "07040014"
               "aaaaaa");

  header = "BC: Normal";
  testCallback(false, test, header, "",
               "aaaaaa"
               "00"
               "fe"  // broadcast
               "0704003b"
               "aaaaaa");
}

void testActive(const bool &external) {
  std::string test = external ? "activeCallback - External" : "activeCallback";
  std::string header;

  header = "BC: Request Bus - Normal";
  testCallback(external, test, header, "feb5050427002d00",
               "aaaaaa"
               "33"  // own Address == Arbitration won
               "aaaaaa");

  header = "BC: Request Bus - Priority lost";
  testCallback(external, test, header, "feb5050427002d00",
               "aaaaaa"
               "01"  // other Address == Priority lost
               "aaaaaa");

  header = "BC: Request Bus - Priority lost/wrong byte";
  testCallback(external, test, header, "feb5050427002d00",
               "aaaaaa"
               "01"  // other Address == Priority lost
               "ab"  // wrong byte
               "aaaaaa");

  header = "BC: Request Bus - Priority fit/won";
  testCallback(external, test, header, "feb5050427002d00",
               "aaaaaa"
               "73"  // own Address == Priority retry
               "aa"
               "33"  // own Address == Arbitration won
               "aaaaaa");

  header = "BC: Request Bus - Priority fit/lost";
  testCallback(external, test, header, "feb5050427002d00",
               "aaaaaa"
               "73"  // own Address == Priority retry
               "aa"
               "13"  // other Address == Arbitration lost
               "aaaaaa");

  header = "BC: Request Bus - Priority retry/error";
  testCallback(external, test, header, "feb5050427002d00",
               "aaaaaa"
               "73"  // own Address == Priority retry
               "a0"  // error
               "aaaaaa");

  header = "MS: Normal";
  testCallback(external, test, header, "52b509030d4600",
               "aaaaaa"
               "33"  // own master address == Arbitration won
               "00"  // Master ACK
               "013fa4"
               "aaaaaa");

  header = "MS: Master NAK/ACK - Slave CRC wrong/correct";
  testCallback(external, test, header, "52b509030d4600",
               "aaaaaa"
               "33"      // own master address == Arbitration won
               "ff"      // Master NAK
               "00"      // Master ACK
               "013fa3"  // Slave CRC wrong
               "013fa4"  // Slave CRC correct
               "aaaaaa");

  header = "MS: Master NAK/ACK - Slave CRC wrong/wrong";
  testCallback(external, test, header, "52b509030d4600",
               "aaaaaa"
               "33"      // own master address == Arbitration won
               "00"      // Master ACK
               "013fa3"  // Slave CRC wrong
               "013fa3"  // Slave CRC wrong
               "aaaaaa");

  header = "MS: Master NAK/NAK";
  testCallback(external, test, header, "52b509030d4600",
               "aaaaaa"
               "33"  // own master address == Arbitration won
               "ff"  // Master NAK
               "ff"  // Master NAK
               "aaaaaa");

  header = "MM: Master NAK/ACK";
  testCallback(external, test, header, "10b57900",
               "aaaaaa"
               "33"  // own Address == Arbitration won
               "ff"  // Master NAK
               "00"  // Master ACK
               "aaaaaa");
}

void errorCallback(const std::string str) {
  std::cout << "   error: " << str << std::endl;
}

void printCounters() {
  ebus::Counters counter = ebusHandler.getCounters();

  // messages
  std::cout << "messagesTotal: " << counter.messagesTotal << std::endl;

  std::cout << "messagesPassiveMS: " << counter.messagesPassiveMS << std::endl;
  std::cout << "messagesPassiveMM: " << counter.messagesPassiveMM << std::endl;

  std::cout << "messagesReactiveMS: " << counter.messagesReactiveMS
            << std::endl;
  std::cout << "messagesReactiveMM: " << counter.messagesReactiveMM
            << std::endl;
  std::cout << "messagesReactiveBC: " << counter.messagesReactiveBC
            << std::endl;

  std::cout << "messagesActiveMS: " << counter.messagesActiveMS << std::endl;
  std::cout << "messagesActiveMM: " << counter.messagesActiveMM << std::endl;
  std::cout << "messagesActiveBC: " << counter.messagesActiveBC << std::endl;

  // errors
  std::cout << "errorsTotal: " << counter.errorsTotal << std::endl;

  std::cout << "errorsPassive: " << counter.errorsPassive << std::endl;
  std::cout << "errorsPassiveMaster: " << counter.errorsPassiveMaster
            << std::endl;
  std::cout << "errorsPassiveMasterACK: " << counter.errorsPassiveMasterACK
            << std::endl;
  std::cout << "errorsPassiveSlave: " << counter.errorsPassiveSlave
            << std::endl;
  std::cout << "errorsPassiveSlaveACK: " << counter.errorsPassiveSlaveACK
            << std::endl;

  std::cout << "errorsReactive: " << counter.errorsReactive << std::endl;
  std::cout << "errorsReactiveMaster: " << counter.errorsReactiveMaster
            << std::endl;
  std::cout << "errorsReactiveMasterACK: " << counter.errorsReactiveMasterACK
            << std::endl;
  std::cout << "errorsReactiveSlave: " << counter.errorsReactiveSlave
            << std::endl;
  std::cout << "errorsReactiveSlaveACK: " << counter.errorsReactiveSlaveACK
            << std::endl;

  std::cout << "errorsActive: " << counter.errorsActive << std::endl;
  std::cout << "errorsActiveMaster: " << counter.errorsActiveMaster
            << std::endl;
  std::cout << "errorsActiveMasterACK: " << counter.errorsActiveMasterACK
            << std::endl;
  std::cout << "errorsActiveSlave: " << counter.errorsActiveSlave << std::endl;
  std::cout << "errorsActiveSlaveACK: " << counter.errorsActiveSlaveACK
            << std::endl;

  // resets
  std::cout << "resetsTotal: " << counter.resetsTotal << std::endl;
  std::cout << "resetsPassive00: " << counter.resetsPassive00 << std::endl;
  std::cout << "resetsPassive: " << counter.resetsPassive << std::endl;
  std::cout << "resetsActive: " << counter.resetsActive << std::endl;

  // requests
  std::cout << "requestsTotal: " << counter.requestsTotal << std::endl;
  std::cout << "requestsWon: " << counter.requestsWon << std::endl;
  std::cout << "requestsLost: " << counter.requestsLost << std::endl;
  std::cout << "requestsRetry: " << counter.requestsRetry << std::endl;
  std::cout << "requestsError: " << counter.requestsError << std::endl;
}

int main() {
  ebusHandler.setErrorCallback(errorCallback);
  // printBytes = true;

  testPassive();
  testReactive();
  testActive(false);
  testActive(true);

  printCounters();

  return (0);
}
