/*
 * Copyright (C) 2025-2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "Core/BusHandler.hpp"
#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "Utils/Common.hpp"

struct TestCase {
  ebus::MessageType messageType;
  uint8_t address;
  std::string description;
  std::string read_string;
  std::string send_string = "";
  struct ExpectedResult {
    int telegrams;
    int errors;
  } expected;
};

// Globals for callbacks
std::atomic<int> g_telegram_count(0);
std::atomic<int> g_error_count(0);
bool g_detailed_output = false;

void telegramCallback(const ebus::MessageType& messageType,
                      const ebus::TelegramType& telegramType,
                      const std::vector<uint8_t>& master,
                      const std::vector<uint8_t>& slave) {
  g_telegram_count++;
  if (g_detailed_output) {
    std::cout << "    Telegram: " << ebus::to_string(master) << " "
              << ebus::to_string(slave) << std::endl;
  }
}

void errorCallback(const std::string& error, const std::vector<uint8_t>& master,
                   const std::vector<uint8_t>& slave) {
  g_error_count++;
  if (g_detailed_output) {
    std::cout << "    Error: " << error << std::endl;
  }
}

bool run_test(const TestCase& tc, ebus::Bus& bus, ebus::Handler& handler,
              ebus::Request& request) {
  g_telegram_count = 0;
  g_error_count = 0;

  // Reset state
  handler.reset();
  request.reset();
  // Clear bus queue by stopping and restarting (simple way to flush)
  // Note: For performance, Queue::clear() would be better if exposed
  bus.getQueue()->clear();

  handler.setSourceAddress(tc.address);

  if (!g_detailed_output) {
    std::cout << "[TEST] " << tc.description << "... " << std::flush;
  } else {
    std::cout << std::endl
              << "=== Test: " << tc.description << " ===" << std::endl;
  }

  if (tc.messageType == ebus::MessageType::active) {
    // Prepare active message
    std::vector<uint8_t> msg = ebus::to_vector(tc.send_string);
    handler.sendActiveMessage(msg);
  } else {
    // Passive/Reactive: Inject the sequence
    std::vector<uint8_t> seq = ebus::to_vector(tc.read_string);
    for (uint8_t b : seq) {
      bus.writeByte(b);
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  // Wait for processing to complete
  int retries = 50;
  while (retries-- > 0) {
    if (g_telegram_count >= tc.expected.telegrams &&
        g_error_count >= tc.expected.errors) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  bool pass = (g_telegram_count == tc.expected.telegrams &&
               g_error_count == tc.expected.errors);

  if (!g_detailed_output) {
    std::cout << (pass ? "PASSED" : "FAILED") << std::endl;
  } else {
    std::cout << "[RESULT] " << (pass ? "PASSED" : "FAILED") << std::endl;
    if (!pass) {
      std::cout << "Expected Tel: " << tc.expected.telegrams
                << ", Err: " << tc.expected.errors
                << ". Got Tel: " << g_telegram_count
                << ", Err: " << g_error_count << "." << std::endl;
    }
  }
  return pass;
}

void test_integration_vectors() {
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime = {
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = true};

  ebus::Request request;
  request.setMaxLockCounter(0);  // Bypass lock for deterministic testing
  ebus::Bus bus(config, runtime, &request);
  ebus::Handler handler(runtime.address, &bus, &request);
  ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

  handler.setTelegramCallback(telegramCallback);
  handler.setErrorCallback(errorCallback);

  // Register write listener for synchronous logging
  bus.addWriteListener([](const uint8_t& byte) {
    if (g_detailed_output)
      std::cout << "<- write: " << ebus::to_string(byte) << std::endl;
  });

  // Read listener
  bus.addReadListener([](const uint8_t& byte) {
    if (g_detailed_output)
      std::cout << "->  read: " << ebus::to_string(byte) << std::endl;
  });

  // Listener to handle Active Master-Slave simulation
  // When we send an active master telegram, we need to inject the slave
  // response after the master CRC is echoed.
  busHandler.addByteListener([&bus, &handler](const uint8_t& byte) {
    // Check if we are active and just finished sending Master CRC
    if (handler.getState() ==
        ebus::HandlerState::activeReceiveMasterAcknowledge) {
      // We just entered this state, meaning we sent the CRC.
      // In simulation, we immediately ACK the master telegram to proceed.
      // But wait, the Handler expects an ACK from the *slave*.
      // Since we simulate the bus, we must provide that ACK.
      // Note: This simple logic assumes "Active MS Normal" test case data.
      static bool handled = false;
      if (!handled) {
        // Inject Slave Response for the "Active MS" test case
        // The test case sends to 08 (0x08).
        // Slave response: 00 (ACK) + 01 (NN) + 3f (Data) + a4 (CRC)
        // We inject this sequence.
        // bus.writeByte(0x00); // ACK done by bus echo? No, ACK is from Slave.
        // Wait, handler writes? No, Handler waits for ACK.
        // So we write ACK.
        // The Handler state machine handles the Master->Slave transition.
        // This is tricky to genericize without full simulation logic.
        // For this test suite, we focus on Passive/Reactive and Active BC.
      }
    }
  });

  bus.start();
  busHandler.start();

  // Test Cases
  // Note: We skip complex Active MS/Arbitration Lost tests here as they require
  // a collision-aware bus simulator. We verify the plumbing with Happy Paths.
  // clang-format off
  std::vector<TestCase> test_cases = {
      {ebus::MessageType::passive, 0x33, "passive MS: Normal", "ff52b509030d0600430003b0fba901d000", "", {1, 0}},
      {ebus::MessageType::passive, 0x33, "passive BC: Normal", "10fe07000970160443183105052592", "", {1, 0}},

      {ebus::MessageType::reactive, 0x33, "reactive BC: Normal", "00fe0704003b", "", {1, 0}},

      {ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "", "feb5050427002d00", {1, 0}},
  };
  // clang-format on

  for (const auto& tc : test_cases) {
    g_detailed_output = false;
    if (!run_test(tc, bus, handler, request)) {
      g_detailed_output = true;
      run_test(tc, bus, handler, request);
      exit(1);
    }
  }

  busHandler.stop();
  bus.stop();
}

void test_lock_counter() {
  std::cout << "[TEST] Lock Counter Logic... " << std::flush;

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0xff, .window = 50, .offset = 5};

  ebus::Request request;
  ebus::Bus bus(config, runtime, &request);
  ebus::Handler handler(runtime.address, &bus, &request);
  ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

  // Setup: Max lock counter 3
  request.setMaxLockCounter(3);

  // We need callbacks to track completion
  g_telegram_count = 0;
  handler.setTelegramCallback(telegramCallback);

  bus.start();
  busHandler.start();

  // 1. Send First Message (Active BC)
  // 33 feb5050427002d00 2c (CRC)
  std::vector<uint8_t> msg = ebus::to_vector("feb5050427002d00");
  handler.sendActiveMessage(msg);

  // Pump SYNs to ensure arbitration win (needs initial lock counter decrement)
  for (int i = 0; i < 5; ++i) {
    bus.writeByte(ebus::sym_syn);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (g_telegram_count == 1) break;
  }

  if (g_telegram_count != 1) {
    std::cout << "FAILED (First msg timed out)" << std::endl;
    exit(1);
  }

  // 2. Verify Lock Counter Reset
  // After success, lockCounter should be reset to max (3)
  // The SYN byte from releaseBus also counts and decrement the lockCounter to 2
  if (request.getLockCounter() != 2) {
    std::cout << "FAILED (Lock counter not reset to 2, got "
              << (int)request.getLockCounter() << ")" << std::endl;
    exit(1);
  }

  // 3. Send Second Message
  g_telegram_count = 0;
  handler.sendActiveMessage(msg);

  // 4. Pump SYNs and check lock counter decrement
  // SYN 1 -> Decrement to 1
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (request.getLockCounter() != 1) {
    std::cout << "FAILED (Lock counter not 1 after 1st SYN, got "
              << (int)request.getLockCounter() << ")" << std::endl;
    exit(1);
  }

  // SYN 2 -> Decrement to 0
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (request.getLockCounter() != 0) {
    std::cout << "FAILED (Lock counter not 0 after 2nd SYN, got "
              << (int)request.getLockCounter() << ")" << std::endl;
    exit(1);
  }

  // SYN 3 -> Request granted (0) -> Arbitration
  bus.writeByte(ebus::sym_syn);

  // Wait for completion
  for (int i = 0; i < 50; ++i) {
    if (g_telegram_count == 1) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (g_telegram_count == 1) {
    std::cout << "PASSED" << std::endl;
  } else {
    std::cout << "FAILED (Second msg timed out)" << std::endl;
    exit(1);
  }

  busHandler.stop();
  bus.stop();
}

void test_external_client() {
  auto run = []() -> bool {
    if (!g_detailed_output) {
      std::cout << "[TEST] External Client Logic... " << std::flush;
    } else {
      std::cout << std::endl
                << "=== Test: External Client Logic ===" << std::endl;
    }

    ebus::busConfig config = {.device = "/dev/null", .simulate = true};
    ebus::RuntimeConfig runtime = {
        .address = 0x33, .window = 50, .offset = 5, .enable_syn = true};

    ebus::Request request;
    request.setMaxLockCounter(0);
    ebus::Bus bus(config, runtime, &request);
    ebus::Handler handler(runtime.address, &bus, &request);
    ebus::BusHandler busHandler(&request, &handler, bus.getQueue());

    // Setup logging
    bus.addWriteListener([](const uint8_t& byte) {
      if (g_detailed_output)
        std::cout << "<- write: " << ebus::to_string(byte) << std::endl;
    });
    bus.addReadListener([](const uint8_t& byte) {
      if (g_detailed_output)
        std::cout << "->  read: " << ebus::to_string(byte) << std::endl;
    });

    g_telegram_count = 0;
    handler.setTelegramCallback(telegramCallback);
    handler.setErrorCallback(errorCallback);

    std::atomic<bool> callbackFired{false};

    // Simulate a RegularClient sending a message after arbitration
    // Message: 33 (Arb) fe b5 05 04 27 00 2d 00 2c (CRC)
    // The 33 is sent by Request/Bus during arbitration.
    // The client sends the rest.
    std::vector<uint8_t> clientData = ebus::to_vector("feb5050427002d002c");

    request.setExternalBusRequestedCallback([&]() {
      callbackFired = true;
      for (uint8_t b : clientData) {
        bus.writeByte(b);
        // Simulate transmission delay
        std::this_thread::sleep_for(std::chrono::microseconds(500));
      }
    });

    bus.start();
    busHandler.start();

    // Wait for request to be granted and telegram processed (driven by SYN gen)
    for (int i = 0; i < 100; ++i) {
      // Retry request until accepted (requires busAvailable, which needs
      // initial SYNs)
      if (!callbackFired && !request.busRequestPending()) {
        request.requestBus(0x33, true);
      }
      if (callbackFired && g_telegram_count == 1) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    bool pass = callbackFired && (g_telegram_count == 1);

    if (!g_detailed_output) {
      std::cout << (pass ? "PASSED" : "FAILED") << std::endl;
    } else {
      std::cout << "[RESULT] " << (pass ? "PASSED" : "FAILED") << std::endl;
      if (!pass) {
        std::cout << "CallbackFired: " << callbackFired
                  << ", TelegramCount: " << g_telegram_count << std::endl;
      }
    }

    busHandler.stop();
    bus.stop();

    return pass;
  };

  g_detailed_output = false;
  if (!run()) {
    g_detailed_output = true;
    run();
    exit(1);
  }
}

int main() {
  test_integration_vectors();
  test_lock_counter();
  test_external_client();

  std::cout << "\nAll bushandler tests passed!" << std::endl;

  return EXIT_SUCCESS;
}