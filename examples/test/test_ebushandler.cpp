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

void testCallback(const std::string &header, const std::string &message,
                  const std::string &sequence) {
  ebus::Sequence seq;
  seq.assign(ebus::Sequence::to_vector(sequence));

  std::cout << " address: " << to_string(ebusHandler.getAddress()) << " ("
            << to_string(ebusHandler.getSlaveAddress()) << ")" << std::endl;
  std::cout << "    name: " << header << std::endl;
  if (message.size() > 0)
    std::cout << " message: " << to_string(ebusHandler.getAddress()) << message
              << std::endl;
  else
    std::cout << "sequence: " << sequence << std::endl;

  ebusHandler.enque(ebus::Sequence::to_vector(message));
  ebusHandler.setMaxLockCounter(3);

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

    ebusHandler.run(seq[i]);
  }

  std::cout << std::endl;
}

void testPassiveCallback() {
  std::string header;
  std::cout << std::endl
            << "    test: passiveCallback" << std::endl
            << std::endl;

  header = "MS: Normal";
  testCallback(header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"  // extended a901 >> aa
               "00"
               "aaaaaa");

  header = "MS: Master NAK/repeat";
  testCallback(header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "ff"                  // Master NAK
               "ff52b509030d060043"  // Master repeat
               "00"
               "03b0fba901d0"
               "00"
               "aaaaaa");

  header = "MS: Master NAK/repeat/NAK";
  testCallback(header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "ff"                  // Master NAK
               "ff52b509030d060043"  // Master repeat
               "ff"                  // Master NAK
               "aaaaaa");

  header = "MS: Slave NAK/repeat";
  testCallback(header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"
               "ff"            // Slave NAK
               "03b0fba901d0"  // Slave repeat
               "00"
               "aaaaaa");

  header = "MS: Slave NAK/repeat/NAK";
  testCallback(header, "",
               "aaaaaa"
               "ff52b509030d060043"
               "00"
               "03b0fba901d0"
               "ff"            // Slave NAK
               "03b0fba901d0"  // Slave repeat
               "ff"            // Slave NAK
               "aaaaaa");

  header = "MS: Master NAK/repeat - Slave NAK/repeat";
  testCallback(header, "",
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
  testCallback(header, "",
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
  testCallback(header, "",
               "aaaaaa"
               "1000b5050427002400d900"
               "aaaaaa");

  header = "BC: defect";
  testCallback(header, "",
               "aaaaaa"
               "00"
               "fe"        // broadcast
               "0704003c"  // defect
               "aaaaaa");
}

void testReactiveCallback() {
  std::string header;
  std::cout << std::endl
            << "    test: reactiveCallback" << std::endl
            << std::endl;

  header = "MS: Slave NAK/ACK";
  testCallback(header, "",
               "aaaaaa"
               "00"
               "38"  // own slave address
               "070400ab"
               "ff"  // Slave NAK
               "00"  // Slave ACK
               "aaaaaa");

  header = "MS: Slave defect";
  testCallback(header, "",
               "aaaaaa"
               "00"
               "38"        // own slave address
               "07050031"  // Slave defect
               "aaaaaa");

  header = "MS: Master defect/correct";
  testCallback(header, "",
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
  testCallback(header, "",
               "aaaaaa"
               "00"
               "33"  // own master address
               "07040014"
               "aaaaaa");

  header = "BC: Normal";
  testCallback(header, "",
               "aaaaaa"
               "00"
               "fe"  // broadcast
               "0704003b"
               "aaaaaa");
}

void testActiveCallback() {
  std::string header;
  std::cout << std::endl
            << "    test: activeCallback" << std::endl
            << std::endl;

  header = "BC: Request Bus - Normal";
  testCallback(header, "feb5050427002d00",
               "aaaaaa"
               "33"  // own Address == Arbitration won
               "aaaaaa");

  header = "BC: Request Bus - Priority lost";
  testCallback(header, "feb5050427002d00",
               "aaaaaa"
               "01"  // other Address == Priority lost
               "aaaaaa");

  header = "BC: Request Bus - Priority lost/wrong byte";
  testCallback(header, "feb5050427002d00",
               "aaaaaa"
               "01"  // other Address == Priority lost
               "ab"  // wrong byte
               "aaaaaa");

  header = "BC: Request Bus - Priority fit/won";
  testCallback(header, "feb5050427002d00",
               "aaaaaa"
               "73"  // own Address == Priority retry
               "aa"
               "33"  // own Address == Arbitration won
               "aaaaaa");

  header = "BC: Request Bus - Priority fit/lost";
  testCallback(header, "feb5050427002d00",
               "aaaaaa"
               "73"  // own Address == Priority retry
               "aa"
               "13"  // other Address == Arbitration lost
               "aaaaaa");

  header = "BC: Request Bus - Priority retry/error";
  testCallback(header, "feb5050427002d00",
               "aaaaaa"
               "73"  // own Address == Priority retry
               "a0"  // error
               "aaaaaa");

  header = "MS: Normal";
  testCallback(header, "52b509030d4600",
               "aaaaaa"
               "33"  // own master address == Arbitration won
               "00"  // Master ACK
               "013fa4"
               "aaaaaa");

  header = "MS: Master NAK/ACK - Slave CRC wrong/correct";
  testCallback(header, "52b509030d4600",
               "aaaaaa"
               "33"      // own master address == Arbitration won
               "ff"      // Master NAK
               "00"      // Master ACK
               "013fa3"  // Slave CRC wrong
               "013fa4"  // Slave CRC correct
               "aaaaaa");

  header = "MS: Master NAK/ACK - Slave CRC wrong/wrong";
  testCallback(header, "52b509030d4600",
               "aaaaaa"
               "33"      // own master address == Arbitration won
               "00"      // Master ACK
               "013fa3"  // Slave CRC wrong
               "013fa3"  // Slave CRC wrong
               "aaaaaa");

  header = "MS: Master NAK/NAK";
  testCallback(header, "52b509030d4600",
               "aaaaaa"
               "33"  // own master address == Arbitration won
               "ff"  // Master NAK
               "ff"  // Master NAK
               "aaaaaa");

  header = "MM: Master NAK/ACK";
  testCallback(header, "10b57900",
               "aaaaaa"
               "33"  // own Address == Arbitration won
               "ff"  // Master NAK
               "00"  // Master ACK
               "aaaaaa");
}

void traceCallback(const char *state) {
  std::cout << " handler:    " << state << std::endl;
}

int main() {
  // ebusHandler.setTraceCallback(traceCallback);
  // printBytes = true;

  testPassiveCallback();
  testReactiveCallback();
  testActiveCallback();

  ebus::Counters counter = ebusHandler.getCounters();

  std::cout << "tolal:               " << counter.total << std::endl;

  std::cout << "passive:             " << counter.passive << std::endl;
  std::cout << "passivePercent:      " << counter.passivePercent << std::endl;

  std::cout << "passiveMS:           " << counter.passiveMS << std::endl;
  std::cout << "passiveMM:           " << counter.passiveMM << std::endl;
  std::cout << "passiveBC:           " << counter.passiveBC << std::endl;

  std::cout << "passiveMSAtMe:       " << counter.passiveMSAtMe << std::endl;
  std::cout << "passiveMMAtMe:       " << counter.passiveMMAtMe << std::endl;

  std::cout << "active:              " << counter.active << std::endl;
  std::cout << "activePercent:       " << counter.activePercent << std::endl;

  std::cout << "activeMS:            " << counter.activeMS << std::endl;
  std::cout << "activeMM:            " << counter.activeMM << std::endl;
  std::cout << "activeBC:            " << counter.activeBC << std::endl;

  std::cout << "failure:             " << counter.failure << std::endl;
  std::cout << "failurePercent:      " << counter.failurePercent << std::endl;

  std::cout << "requestTotal:        " << counter.requestTotal << std::endl;

  std::cout << "requestWon1:         " << counter.requestWon1 << std::endl;
  std::cout << "requestWon2:         " << counter.requestWon2 << std::endl;

  std::cout << "requestLost1:        " << counter.requestLost1 << std::endl;
  std::cout << "requestLost2:        " << counter.requestLost2 << std::endl;

  std::cout << "requestRetry:        " << counter.requestRetry << std::endl;
  std::cout << "requestError:        " << counter.requestError << std::endl;

  std::cout << "requestWonPercent:   " << counter.requestWonPercent
            << std::endl;
  std::cout << "requestLostPercent:  " << counter.requestLostPercent
            << std::endl;
  std::cout << "requestErrorPercent: " << counter.requestErrorPercent
            << std::endl;

  return (0);
}
