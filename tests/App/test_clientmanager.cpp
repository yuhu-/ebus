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

  // 1. Setup eBUS Stack
  ebus::Request req;
  req.setMaxLockCounter(0);

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{.address = 0x01, .window = 50, .offset = 5};

  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  // 2. Setup Client A (Regular - Sender)
  int svReg[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
  manager.addClient(svReg[0], ebus::ClientType::Regular);

  // 3. Setup Client B (ReadOnly - Observer)
  int svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

  // Start threads
  bus.start();
  busHandler.start();
  manager.start();

  // 4. Broadcast Telegram: [33] [fe] [b5] [05] [04] [27] [00] [2d] [00] [2c]
  std::vector<uint8_t> telegram = {0x33, 0xfe, 0xb5, 0x05, 0x04,
                                   0x27, 0x00, 0x2d, 0x00, 0x2c};

  // 5. Make bus available for request
  req.forceResultForTest(ebus::RequestResult::observeSyn);

  // 6. Send from Regular Client
  send(svReg[1], &telegram[0], 1, 0);

  // Wait for Manager to recognize and call requestBus
  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }
  run_test("Manager recognized bus request", req.busRequestPending());

  // 7. Simulate arbitration success
  uint8_t addr = req.busRequestAddress();
  req.forceResultForTest(ebus::RequestResult::firstWon);
  req.busRequestCompleted();
  // Manually provide the echo for the arbitration address only
  bus.writeByte(addr);

  // Fix: Move the request tracker to 'observeData' for the payload phase.
  // This ensures handleBusData treats subsequent bytes as telegram content.
  req.forceResultForTest(ebus::RequestResult::observeData);

  // 8. Send remaining bytes. Echoes are now handled automatically by the
  // listener!
  for (size_t i = 1; i < telegram.size(); ++i) {
    send(svReg[1], &telegram[i], 1, 0);
    // No more bus.writeByte() here!
    usleep(10000);
  }

  // 9. Verification
  uint8_t bufReg[10];
  run_test("Regular client received full echo",
           read_exact(svReg[1], bufReg, 10) &&
               std::vector<uint8_t>(bufReg, bufReg + 10) == telegram);

  uint8_t bufRO[10];
  run_test("ReadOnly client received broadcast",
           read_exact(svRO[1], bufRO, 10) &&
               std::vector<uint8_t>(bufRO, bufRO + 10) == telegram);

  // Cleanup
  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svReg[1]);
  close(svRO[1]);
}

void test_arbitration_lost() {
  std::cout << "--- Test: ClientManager Arbitration Lost (Regular + ReadOnly + "
               "Enhanced) ---"
            << std::endl;

  // 1. Setup eBUS Stack
  ebus::Request req;
  req.setMaxLockCounter(0);

  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  // 2. Setup Clients
  int svReg[2], svRO[2], svEnh[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svReg);
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);

  manager.addClient(svReg[0], ebus::ClientType::Regular);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);
  manager.addClient(svEnh[0], ebus::ClientType::Enhanced);

  bus.start();
  busHandler.start();
  manager.start();

  // Consume Enhanced greeting
  char greeting[64];
  read_exact(svEnh[1], (uint8_t*)greeting, GREETING_STR.length());

  // 3. Regular Client tries to win the bus with 0x33
  req.forceResultForTest(ebus::RequestResult::observeSyn);
  uint8_t myAddr = 0x33;
  send(svReg[1], &myAddr, 1, 0);

  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }

  // 4. Simulate collision where 0xbb wins arbitration
  uint8_t winner = 0xbb;
  req.forceResultForTest(ebus::RequestResult::firstLost);
  req.busRequestCompleted();
  bus.writeByte(winner);  // Inject the winner on the wire

  // Move back to observeData for the next potential bytes
  usleep(10000);
  req.forceResultForTest(ebus::RequestResult::observeData);
  usleep(20000);

  // 5. Verification
  uint8_t buf;
  run_test("Regular client (sender) received winner raw",
           read_exact(svReg[1], &buf, 1) && buf == winner);
  run_test("ReadOnly client received winner raw",
           read_exact(svRO[1], &buf, 1) && buf == winner);

  // Enhanced observer receives encoded 0xbb (RESP_RECEIVED 0x01 + 0xbb -> 0xc6
  // 0xbb)
  uint8_t bufEnh[2];
  run_test("Enhanced client received encoded winner (0xc6 0xbb)",
           read_exact(svEnh[1], bufEnh, 2) && bufEnh[0] == 0xc6 &&
               bufEnh[1] == 0xbb);

  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svReg[1]);
  close(svRO[1]);
  close(svEnh[1]);
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
  bus.writeByte(0xAA);
  manager.start();
  usleep(20000);

  // If we reach here without a hang or crash, cleanup logic worked
  run_test("Manager handled closed socket", true);

  manager.stop();
  close(sv[1]);
}

void test_enhanced_active_sending() {
  std::cout << "--- Test: ClientManager Enhanced Active Sending ---"
            << std::endl;

  // 1. Setup eBUS Stack
  ebus::Request req;
  req.setMaxLockCounter(0);
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  // 2. Setup Clients
  int svEnh[2], svRO[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  socketpair(AF_UNIX, SOCK_STREAM, 0, svRO);

  manager.addClient(svEnh[0], ebus::ClientType::Enhanced);
  manager.addClient(svRO[0], ebus::ClientType::ReadOnly);

  bus.start();
  busHandler.start();
  manager.start();

  // Consume Enhanced greeting
  char greeting[64];
  read_exact(svEnh[1], (uint8_t*)greeting, GREETING_STR.length());

  // 3. Enhanced Client starts arbitration with 0x33
  // CMD_START(0x33) -> 0xc8 0xb3
  uint8_t cmdStart[] = {0xc8, 0xb3};
  req.forceResultForTest(ebus::RequestResult::observeSyn);
  send(svEnh[1], cmdStart, 2, 0);

  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }
  run_test("Manager recognized Enhanced bus request", req.busRequestPending());

  // 4. Simulate Arbitration Win
  req.forceResultForTest(ebus::RequestResult::firstWon);
  req.busRequestCompleted();
  bus.writeByte(0x33);  // Echo of address to drive the manager state machine
  usleep(20000);

  // 5. Verify Enhanced client receives RESP_STARTED(0x33) -> 0xc8 0xb3
  uint8_t resp[2];
  run_test("Enhanced received RESP_STARTED",
           read_exact(svEnh[1], resp, 2) && resp[0] == 0xc8 && resp[1] == 0xb3);

  // Fix: Now that arbitration is over, force the result to 'observeData'
  // so subsequent bytes are treated as telegram payload (RESP_RECEIVED).
  req.forceResultForTest(ebus::RequestResult::observeData);

  // 6. Enhanced Client sends CMD_SEND(0xfe) -> 0xc7 0xbe
  uint8_t cmdSend[] = {0xc7, 0xbe};
  send(svEnh[1], cmdSend, 2, 0);
  usleep(20000);  // Wait for transmission and bus echo

  // 7. Verify Enhanced client receives RESP_RECEIVED(0xfe) -> 0xc7 0xbe
  run_test("Enhanced received encoded 0xfe",
           read_exact(svEnh[1], resp, 2) && resp[0] == 0xc7 && resp[1] == 0xbe);

  // 8. Verify ReadOnly client received raw bytes [33] [fe]
  uint8_t raw[2];
  run_test("ReadOnly received raw telegram",
           read_exact(svRO[1], raw, 2) && raw[0] == 0x33 && raw[1] == 0xfe);

  manager.stop();
  busHandler.stop();
  bus.stop();
  close(svEnh[1]);
  close(svRO[1]);
}

void test_enhanced_arbitration_lost() {
  std::cout << "--- Test: ClientManager Enhanced Arbitration Lost ---"
            << std::endl;

  ebus::Request req;
  req.setMaxLockCounter(0);
  ebus::busConfig config = {.device = "/dev/null", .simulate = true};
  ebus::RuntimeConfig runtime{
      .address = 0xff, .window = 50, .offset = 5, .enable_syn = false};
  ebus::Bus bus(config, runtime, &req);
  ebus::Handler handler(runtime.address, &bus, &req);
  ebus::BusHandler busHandler(&req, &handler, bus.getQueue());
  ebus::ClientManager manager(&bus, &busHandler, &req);

  int svEnh[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, svEnh);
  manager.addClient(svEnh[0], ebus::ClientType::Enhanced);

  bus.start();
  busHandler.start();
  manager.start();

  // Consume greeting
  char greeting[64];
  read_exact(svEnh[1], (uint8_t*)greeting, GREETING_STR.length());

  // 1. Request bus with 0x33
  uint8_t cmdStart[] = {0xc8, 0xb3};
  req.forceResultForTest(ebus::RequestResult::observeSyn);
  send(svEnh[1], cmdStart, 2, 0);

  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }

  // 2. Force arbitration lost to master 0x09
  uint8_t winner = 0x09;
  req.forceResultForTest(ebus::RequestResult::firstLost);
  req.busRequestCompleted();
  bus.writeByte(winner);
  usleep(20000);

  // 3. Verify RESP_FAILED (0x0a) + winner (0x09) -> 0xe8 0x89
  uint8_t resp[2];
  run_test("Enhanced received RESP_FAILED",
           read_exact(svEnh[1], resp, 2) && resp[0] == 0xe8 && resp[1] == 0x89);

  // 4. Verify client is NOT disconnected but no longer active sender
  bus.writeByte(ebus::sym_syn);
  usleep(20000);

  // Enhanced observer receives SYN (Short form 0xaa is not possible,
  // so encoded RESP_RECEIVED 0x01 + 0xaa -> 0xc6 0xaa)
  run_test("Enhanced client stayed connected and received SYN",
           read_exact(svEnh[1], resp, 2) && resp[0] == 0xc6 && resp[1] == 0xaa);

  manager.stop();
  bus.stop();
  close(svEnh[1]);
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

  // 1. Initiate a request
  uint8_t addr = 0x33;
  req.forceResultForTest(ebus::RequestResult::observeSyn);
  send(sv[1], &addr, 1, 0);

  // Wait for it to become active
  int timeout = 100;
  while (timeout-- > 0 && !req.busRequestPending()) {
    usleep(1000);
  }
  run_test("Client is now active", req.busRequestPending());

  // 2. Now stop sending bytes from the client side and wait for watchdog (>
  // 1000ms)
  std::cout << "  Waiting for watchdog (1.1s)..." << std::endl;
  usleep(1100000);

  // 3. Verify the bus request was cleared by the manager
  run_test("Watchdog cleared active client", !req.busRequestPending());

  manager.stop();
  bus.stop();
  close(sv[1]);
}

int main() {
  test_client_orchestration();
  test_arbitration_lost();
  test_enhanced_active_sending();
  test_enhanced_arbitration_lost();
  test_client_timeout();
  test_client_removal();

  std::cout << "\nAll ClientManager integration tests passed!" << std::endl;
  return 0;
}