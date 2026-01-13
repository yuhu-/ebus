/*
 * Copyright (C) 2025-2026 Roland Jax
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

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "Bus.hpp"
#include "Common.hpp"
#include "Handler.hpp"
#include "Queue.hpp"
#include "Request.hpp"
#include "Sequence.hpp"
#include "ServiceRunner.hpp"

#define DELAY_10_BIT 4167   // 4167us = 2400 baud, 10 bit
#define DELAY_REQUEST 4300  // 4300us

struct TestCase {
  bool enabled;
  ebus::MessageType messageType;
  uint8_t address;
  std::string description;
  std::string read_string;
  std::string send_string = "";
};

std::atomic<bool> running(true);

enum class CallbackType { telegram, error };

struct CallbackEvent {
  CallbackType type;
  struct {
    ebus::MessageType messageType;
    ebus::TelegramType telegramType;
    std::vector<uint8_t> master;
    std::vector<uint8_t> slave;
    std::string error;
  } data;
};

using EventQueue = ebus::Queue<CallbackEvent>;

void reactiveMasterSlaveCallback(const std::vector<uint8_t> &master,
                                 std::vector<uint8_t> *const slave) {
  std::vector<uint8_t> search;
  search = {0x07, 0x04};  // 0008070400
  if (ebus::contains(master, search))
    *slave = ebus::to_vector("0ab5504d53303001074302");
  search = {0x07, 0x05};  // 0008070500
  if (ebus::contains(master, search))
    *slave = ebus::to_vector("0ab5504d533030010743");  // defect

  std::cout << "reactive: " << ebus::to_string(master) << " "
            << ebus::to_string(*slave) << std::endl;
}

void callbackRunnerTask(EventQueue *evenQueue, std::atomic<bool> &running) {
  CallbackEvent event;
  while (running) {
    if (evenQueue && evenQueue->pop(event)) {
      switch (event.type) {
        case CallbackType::error:
          std::cout << "   error: " << event.data.error << " : master '"
                    << ebus::to_string(event.data.master) << "' slave '"
                    << ebus::to_string(event.data.slave) << "'" << std::endl;
          break;
        case CallbackType::telegram:
          switch (event.data.telegramType) {
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
          switch (event.data.messageType) {
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
          std::cout << ebus::to_string(event.data.master) << " "
                    << ebus::to_string(event.data.slave) << std::endl;
          break;
      }
    }
  }
}

// Helper to run a test with a given hex string and description
void run_test(const TestCase &tc) {
  std::cout << std::endl
            << "=== Test: " << tc.description << " ===" << std::endl;

  ebus::Bus bus;
  ebus::Request request;
  ebus::Queue<uint8_t> byteQueue(32);
  ebus::Handler handler(tc.address, &bus, &request);

  handler.setReactiveMasterSlaveCallback(reactiveMasterSlaveCallback);

  ebus::Queue<CallbackEvent> eventQueue(8);

  handler.setTelegramCallback(
      [&eventQueue](const ebus::MessageType &messageType,
                    const ebus::TelegramType &telegramType,
                    const std::vector<uint8_t> &master,
                    const std::vector<uint8_t> &slave) {
        CallbackEvent event;
        event.type = CallbackType::telegram;
        event.data.messageType = messageType;
        event.data.telegramType = telegramType;
        event.data.master = master;
        event.data.slave = slave;
        eventQueue.try_push(event);
      });

  handler.setErrorCallback([&eventQueue](const std::string &error,
                                         const std::vector<uint8_t> &master,
                                         const std::vector<uint8_t> &slave) {
    CallbackEvent event;
    event.type = CallbackType::error;
    event.data.error = error;
    event.data.master = master;
    event.data.slave = slave;
    eventQueue.try_push(event);
  });

  // if (tc.messageType == ebus::MessageType::active)
  //   request.requestBus(tc.address);

  ebus::ServiceRunner serviceRunner(request, handler, byteQueue);

  // Register a ByteListener that logs every byte processed by the serviceRunner
  serviceRunner.addByteListener([](const uint8_t &byte) {
    std::cout << "->  read: " << ebus::to_string(byte) << std::endl;
  });

  serviceRunner.enableTesting();
  serviceRunner.start();

  std::thread handlerEventTask(callbackRunnerTask, &eventQueue,
                               std::ref(running));

  // Prepare test sequence from the provided hex string
  std::string tmp = "aaaaaa" + tc.read_string + "aaaaaa";
  ebus::Sequence seq;
  seq.assign(ebus::to_vector(tmp));

  handler.sendActiveMessage(ebus::to_vector(tc.send_string));

  // Simulate ISR: pushes bytes into the byteQueue asynchronously
  // 2400 baud => 1 / 2400 = 416,67 microseconds per bit
  // 10 bits per byte (1 start bit, 8 data bits, 1 stop bit)
  // 1 byte takes 10 * 416,67 microseconds = 4166,67 microseconds
  std::thread ebusUartEventTask([&seq, &bus, &request, &byteQueue, &handler]() {
    for (size_t i = 0; i < seq.size(); ++i) {
      uint8_t byte = seq[i];

      byteQueue.push(byte);

      // simulate request bus timer
      std::this_thread::sleep_for(std::chrono::microseconds(200));
      if (seq[i] == ebus::sym_syn && request.busRequestPending()) {
        std::cout << " ISR - write address" << std::endl;
        bus.writeByte(request.getAddress());
        request.busRequestCompleted();
      }

      // simulate transmission time 4166,67 ~ 4200 microseconds
      std::this_thread::sleep_for(std::chrono::microseconds(4200));
    }
  });

  // Let the serviceRunner process for a short while
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  serviceRunner.stop();
  if (ebusUartEventTask.joinable()) ebusUartEventTask.join();

  running = false;
  if (handlerEventTask.joinable()) handlerEventTask.join();

  std::cout << " written: " << bus.getWrittenBytesString() << std::endl;

  std::cout << "--- Test: " << tc.description << " ---" << std::endl;
}

// clang-format off
std::vector<TestCase> test_cases = {
    {false, ebus::MessageType::passive, 0x33, "passive MS: Normal", "ff52b509030d0600430003b0fba901d000"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Master defect/NAK", "ff52b509030d060044ffff52b509030d0600430003b0fba901d000"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat", "ff52b509030d060043ffff52b509030d0600430003b0fba901d000"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat/NAK", "ff52b509030d060043ffff52b509030d060043ffff52b509030d060043ff"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Slave defect/NAK/repeat", "ff52b509030d060044ffff52b509030d0600430003b0fba901d000"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Slave NAK/repeat/NAK", "ff52b509030d0600430003b0fba901d0ff03b0fba901d0ff"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat - Slave NAK/repeat", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d000"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: Master NAK/repeat/ACK - Slave NAK/repeat/NAK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d0ff"},
    {false, ebus::MessageType::passive, 0x33, "passive MM: Normal", "1000b5050427002400d900"},
    {false, ebus::MessageType::passive, 0x33, "passive BC: defect", "00fe0704003c"},
    {false, ebus::MessageType::passive, 0x33, "passive 00: reset", "00"},
    {false, ebus::MessageType::passive, 0x33, "passive 0704: scan", "002e0704004e"},
    {false, ebus::MessageType::passive, 0x33, "passive BC: normal", "10fe07000970160443183105052592"},
    {false, ebus::MessageType::passive, 0x33, "passive MS: slave CRC byte is invalid", "1008b5130304cd017f000acd01000000000100010000"},

    {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/ACK", "0038070400ab000ab5504d5330300107430246ff0ab5504d533030010743024600"},
    {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/NAK", "0038070400ab000ab5504d5330300107430246ff0ab5504d5330300107430246ff"},
    {false, ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/correct", "0038070400acff0038070400ab000ab5504d533030010743024600"},
    {false, ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/defect", "0038070400acff0038070400acff"},
    {false, ebus::MessageType::reactive, 0x33, "reactive MS: Slave defect (callback)", "003807050030aa"},
    {false, ebus::MessageType::reactive, 0x33, "reactive MM: Normal", "003307040014"},
    {false, ebus::MessageType::reactive, 0x33, "reactive BC: Normal", "00fe0704003b"},

    {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "33feb5050427002d00", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost", "01feb5050427002d007b", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost/wrong byte", "01ab", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/won", "73aa33feb5050427002d00", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/lost", "73aa13", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority retry/error", "73a0", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x33, "active MS: Normal", "3352b509030d46003600013fa4", "52b509030d4600"},
    {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/ACK - Slave CRC wrong/correct", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa4", "52b509030d4600"},
    {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/ACK - Slave CRC wrong/wrong", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa3ff", "52b509030d4600"},
    {false, ebus::MessageType::active, 0x33, "active MS: Master NAK/NAK", "3352b509030d460036ff3352b509030d460036ff", "52b509030d4600"},
    {false, ebus::MessageType::active, 0x33, "active MM: Master NAK/ACK", "3310b57900fbff3310b57900fb00", "10b57900"},
    {false, ebus::MessageType::active, 0x30, "active BC: Request Bus - Priority lost and Sub lost", "1052b50401314b000200002c00", "feb5050427002d00"},
    {false, ebus::MessageType::active, 0x30, "active MS: Request Bus - Priority lost to 0x10", "1052b50401314b000200002c00","feb5050427002d00"}

};
// clang-format on

void enable_group(const ebus::MessageType &messageType) {
  for (TestCase &tc : test_cases)
    if (tc.messageType == messageType) tc.enabled = true;
}

int main() {
  enable_group(ebus::MessageType::passive);
  enable_group(ebus::MessageType::reactive);
  enable_group(ebus::MessageType::active);

  for (const TestCase &tc : test_cases)
    if (tc.enabled) run_test(tc);

  return EXIT_SUCCESS;
}
