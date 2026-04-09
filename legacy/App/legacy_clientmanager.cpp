/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/socket.h>
#include <unistd.h>

#include <ebus/Definitions.hpp>
#include <iostream>
#include <vector>

#include "App/ClientManager.hpp"
#include "Core/BusHandler.hpp"
#include "Core/Handler.hpp"
#include "Core/Request.hpp"
#include "Platform/Bus.hpp"
#include "TestUtils.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_client_orchestration() {
  std::cout << "--- Test: ClientManager Orchestration (Regular + ReadOnly) ---"
            << std::endl;

  // Setup eBUS Stack
  ebus::Request req;
  req.setMaxLockCounter(3);  // Start with a standard lock counter for the test
  req.reset();

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0x01, .window = 50, .offset = 5, .enable_syn = false};

  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  // Setup Client A (Regular - Sender)
  // svReg[0] is the manager's end, svReg[1] is the client's end
  int svReg[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
  manager.addClient(svReg[0], ebus::ClientType::Regular);

  // Setup Client B (ReadOnly - Observer)
  // svRO[0] is the manager's end, svRO[1] is the client's end
  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

  // bus.addWriteListener([](const uint8_t& byte) {
  //   std::cout << "Bus <- write: " << ebus::to_string(byte) << std::endl;
  //   std::flush(std::cout);
  // });

  // bus.addReadListener([](const uint8_t& byte) {
  //   std::cout << "Bus ->  read: " << ebus::to_string(byte) << std::endl;
  //   std::flush(std::cout);
  // });

  // busHandler.addByteListener([](const ebus::BusEventContext& ctx) {
  //   std::cout << "BusHandler: " << ebus::to_string(ctx.byte)
  //             << ", State: " << getRequestStateText(ctx.state)
  //             << ", Result: " << getRequestResultText(ctx.result)
  //             << ", LockCounter: " << static_cast<int>(ctx.lockCounter)
  //             << std::endl;
  //   std::flush(std::cout);
  // });

  // Start threads
  bus.start();
  busHandler.start();
  manager.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Broadcast Telegram: [33] [fe] [b5] [05] [04] [27] [00] [2d] [00] [2c]
  std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                   0x27, 0x00, 0x2d, 0x00, 0x2c};

  // Initial SYN to trigger bus processing
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Step 1: Client sends the master address to its local socket.
  send(svReg[1], &telegram[0], 1, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Step 2: Pump SYNs to clear the lock counter
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Step 3: Wait for arbitration to resolve and win.
  run_test("Arbitration resolved and won",
           req.getResult() == ebus::RequestResult::firstWon);
  run_test("LockCounter reset to max", req.getLockCounter() == 3);

  // Step 4: Verify echoes and stream the telegram body
  uint8_t echo;

  // Regular client receives echoes of the manual SYNs first
  // We receive 2 SYN echoes before the address byte echo, because from the
  // point we send the address byte, the client is in Request state and all
  // bytes (including SYNs) are suppressed until we allow the bus request.
  for (int i = 0; i < 3; ++i) read_exact(svReg[1], &echo, 1);

  for (size_t i = 1; i < telegram.size(); ++i) {
    send(svReg[1], &telegram[i], 1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Because it's bus-driven, each byte we send is triggered by the
    // reception/echo of the previous byte.
    run_test("Client received correct byte echo",
             read_exact(svReg[1], &echo, 1) && echo == telegram[i]);
  }

  // Final Verification: ReadOnly observer should have captured everything (4
  // SYNs + 10 bytes telegram)
  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa};
  expectedRO.insert(expectedRO.end(), telegram.begin(), telegram.end());
  std::vector<uint8_t> actualRO(expectedRO.size());
  run_test("ReadOnly client received full trace",
           read_exact(svRO[1], actualRO.data(), actualRO.size()) &&
               actualRO == expectedRO);

  // Cleanup
  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svReg[1]);
  close(svRO[1]);
}

void test_enhanced_active_sending() {
  std::cout << "--- Test: ClientManager Enhanced Active Sending ---"
            << std::endl;

  // Setup eBUS Stack
  ebus::Request req;
  req.setMaxLockCounter(3);  // Start with a standard lock counter for the test
  req.reset();

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0x01, .window = 50, .offset = 5};

  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  // Setup Clients
  int svEnh[2], svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);

  manager.addClient(svEnh[0], ebus::ClientType::Enhanced);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

  // Start threads
  bus.start();
  busHandler.start();
  manager.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Initial SYN to trigger bus processing
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Enhanced Client starts arbitration with 0x33
  // CMD_START(0x33) -> 0xc8 0xb3
  uint8_t cmdStart[] = {0xc8, 0xb3};
  send(svEnh[1], cmdStart, 2, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Step 2: Pump SYNs to clear the lock counter
  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  bus.writeByte(ebus::sym_syn);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Step 3: Wait for arbitration to resolve and win.
  run_test("Arbitration resolved and won",
           req.getResult() == ebus::RequestResult::firstWon);
  run_test("LockCounter reset to max", req.getLockCounter() == 3);

  // Verify Enhanced client receives RESP_STARTED(0x33) -> 0xc8 0xb3
  uint8_t resp[2];

  // Regular client receives echoes of the manual SYNs first
  // We receive 2 SYN echoes before the address byte echo, because from the
  // point we send the address byte, the client is in Request state and all
  // bytes (including SYNs) are suppressed until we allow the bus request.
  for (int i = 0; i < 2; ++i) {
    read_exact(svEnh[1], resp, 2);
    run_test("Enhanced received correct SYN echo",
             (resp[0] == 0xc6 && resp[1] == 0xaa));
  }

  run_test("Enhanced received RESP_STARTED",
           read_exact(svEnh[1], resp, 2) && resp[0] == 0xc8 && resp[1] == 0xb3);

  // Enhanced Client sends CMD_SEND(0xfe) -> 0xc7 0xbe
  uint8_t cmdSend[] = {0xc7, 0xbe};
  send(svEnh[1], cmdSend, 2, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Verify Enhanced client receives RESP_RECEIVED(0xfe) -> 0xc7 0xbe
  run_test("Enhanced received encoded 0xfe",
           read_exact(svEnh[1], resp, 2) && resp[0] == 0xc7 && resp[1] == 0xbe);

  // Verify ReadOnly client received raw bytes
  std::vector<uint8_t> expectedRO = {0xaa, 0xaa, 0xaa, 0xaa, 0x33, 0xfe};
  std::vector<uint8_t> actualRO(expectedRO.size());
  run_test("ReadOnly client received full trace",
           read_exact(svRO[1], actualRO.data(), actualRO.size()) &&
               actualRO == expectedRO);

  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svEnh[1]);
  close(svRO[1]);
}

void test_client_timeout() {
  std::cout << "--- Test: ClientManager Watchdog Timeout ---" << std::endl;

  ebus::Request req;
  req.setMaxLockCounter(0);
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::BusHandler busHandler(&req, nullptr, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(sv[0], ebus::ClientType::Regular);

  bus.start();
  manager.start();

  // Initiate a request
  uint8_t addr = 0x33;
  send(sv[1], &addr, 1, 0);

  // Wait for it to become active
  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }
  run_test("Client is now active", req.busRequestPending());

  // Now stop sending bytes from the client side and wait for watchdog (>
  // 1000ms)
  std::cout << "  Waiting for watchdog (1.1s)..." << std::endl;
  usleep(1100000);

  // Verify the bus request was cleared by the manager
  run_test("Watchdog cleared active client", !req.busRequestPending());

  manager.stop();
  bus.stop();
  close(sv[1]);
}

void test_client_removal() {
  std::cout << "--- Test: Client Removal ---" << std::endl;
  ebus::Request req;
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::BusHandler busHandler(&req, nullptr, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int sv[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  manager.addClient(sv[1], ebus::ClientType::Regular);

  // Closing the other end should trigger disconnected state
  close(sv[0]);

  // Feed some data to trigger the cleanup loop in manager
  bus.writeByte(0xaa);
  manager.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // If we reach here without a hang or crash, cleanup logic worked
  run_test("Manager handled closed socket", true);

  manager.stop();
  close(sv[1]);
}

int main() {
  test_client_orchestration();
  test_enhanced_active_sending();
  test_client_timeout();
  test_client_removal();

  std::cout << "\nAll ClientManager integration tests passed!" << std::endl;
  return 0;
}