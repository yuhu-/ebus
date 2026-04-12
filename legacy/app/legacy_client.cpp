/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <ebus/definitions.hpp>
#include <iostream>
#include <vector>

#include "app/client.hpp"
#include "app/enhanced_protocol.hpp"
#include "core/request.hpp"
#include "test_utils.hpp"

void run_test(const std::string& name, bool condition) {
  std::cout << "[TEST] " << name << ": " << (condition ? "PASSED" : "FAILED")
            << std::endl;
  if (!condition) std::exit(1);
}

void test_readonly_client() {
  std::cout << "--- Test: ReadOnly Client ---" << std::endl;
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    perror("socketpair");
    exit(1);
  }

  ebus::Request req;
  ebus::ReadOnlyClient client(sv[0], &req);

  run_test("ReadOnly is not write capable", !client.isWriteCapable());
  run_test("ReadOnly wantsToSend() is always false", !client.wantsToSend());

  close(sv[0]);
  close(sv[1]);
}

void test_enhanced_client_protocol() {
  std::cout << "--- Test: Enhanced Client Protocol (ebusd binary) ---"
            << std::endl;

  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    perror("socketpair");
    exit(1);
  }

  ebus::Request req;
  ebus::EnhancedClient client(sv[0], &req);

  // Test 1: Simple data byte (< 0x80)
  uint8_t out;
  uint8_t data = 0x15;
  send(sv[1], &data, 1, 0);
  run_test("Read simple byte", client.recvFromClient(out) && out == 0x15);

  // Test 2: Enhanced Escape Sequence (CMD_SEND 0x01)
  // Logical: CMD_SEND (1), Val 0xaa
  uint8_t escaped[2];
  ebus::enhanced::Protocol::encode(0x01, 0xaa, escaped);
  send(sv[1], escaped, 2, 0);
  run_test("Read escaped 0xaa", client.recvFromClient(out) && out == 0xaa);

  // Test 3: CMD_INIT decoding and response
  uint8_t init_cmd[2];
  ebus::enhanced::Protocol::encode(0x00, 0x00,
                                   init_cmd);  // Logical: CMD_INIT (0), val 0
  send(sv[1], init_cmd, 2, 0);
  run_test("Read CMD_INIT (returns false)", !client.recvFromClient(out));

  // Verify library sends RESP_RESETTED (Logical 0x00, val 0x00 -> Encoded 0xc0,
  // 0x80)
  uint8_t init_resp[2];
  run_test("Received RESP_RESETTED", readExact(sv[1], init_resp, 2) &&
                                         init_resp[0] == 0xc0 &&
                                         init_resp[1] == 0x80);

  close(sv[0]);
  close(sv[1]);
}

void test_enhanced_client_responses() {
  std::cout << "--- Test: Enhanced Client Responses (Encoding) ---"
            << std::endl;

  // Create a socket pair for testing
  // sv[0] is the internal client
  // sv[1] represents the external client
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    perror("socketpair");
    exit(1);
  }

  ebus::Request req;
  req.setMaxLockCounter(0);  // Ensure deterministic request result for testing
  req.reset();
  ebus::EnhancedClient client(sv[0], &req);

  // 1. Test: Arbitration Win (driven via Request FSM)
  if (req.busAvailable()) req.requestBus(0x33, true);
  req.busRequestCompleted();
  req.run(0x33);  // FSM sets result to firstWon

  client.onBusByte({0x33, req.getState(), req.getResult(), req.getLockCounter(),
                    std::chrono::steady_clock::now()});

  // Verify RESP_STARTED (Logical 0x02, val 0x33 -> Encoded 0xc8, 0xb3)
  uint8_t resp[2];
  run_test("Encoded RESP_STARTED",
           readExact(sv[1], resp, 2) && resp[0] == 0xc8 && resp[1] == 0xb3);

  // 2. Test: Observation (Short Form < 0x80)
  req.run(0x15);  // FSM sets result to observeData
  client.onBusByte({0x15, req.getState(), req.getResult(), req.getLockCounter(),
                    std::chrono::steady_clock::now()});

  uint8_t short_resp;
  run_test("Received short form RESP_RECEIVED",
           readExact(sv[1], &short_resp, 1) && short_resp == 0x15);

  // 3. Test: Observation (Long Form >= 0x80)
  req.run(0xaa);  // FSM sets result to observeSyn
  client.onBusByte({0xaa, req.getState(), req.getResult(), req.getLockCounter(),
                    std::chrono::steady_clock::now()});

  run_test("Received encoded long RESP_RECEIVED",
           readExact(sv[1], resp, 2) && resp[0] == 0xc6 && resp[1] == 0xaa);

  close(sv[0]);
  close(sv[1]);
}

void test_enhanced_client_invalid_protocol() {
  std::cout << "--- Test: Enhanced Client Invalid Protocol ---" << std::endl;

  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    perror("socketpair");
    exit(1);
  }

  ebus::Request req;
  ebus::EnhancedClient client(sv[0], &req);

  uint8_t out;
  // std::vector<uint8_t> response_buffer(2); // Not used

  // Test 1: Invalid first byte prefix (e.g., starts with 10 instead of 11)
  // Send 0x80 (10xxxxxx) 0xaa (10xxxxxx)
  uint8_t invalid_b1_prefix[] = {0x80, 0xaa};
  send(sv[1], invalid_b1_prefix, 2, 0);
  run_test("Invalid B1 prefix: recvFromClient returns false",
           !client.recvFromClient(out));

  run_test("Invalid B1 prefix: client is disconnected", !client.isConnected());
  // Verify error response
  uint8_t err_resp[2];
  run_test("Invalid B1 prefix: received 2 bytes",
           readExact(sv[1], err_resp, 2));
  // RESP_ERROR_HOST (0x0c), ERR_FRAMING (0x00) -> 0xf0, 0x80
  run_test("Invalid B1 prefix: received encoded RESP_ERROR_HOST",
           err_resp[0] == 0xf0);
  run_test("Invalid B1 prefix: received encoded ERR_FRAMING",
           err_resp[1] == 0x80);

  // Re-establish client for next test (or create a new one)
  close(sv[0]);
  close(sv[1]);
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
    perror("socketpair");
    exit(1);
  }
  ebus::EnhancedClient client2(sv[0], &req);

  // Test 2: Invalid second byte prefix (e.g., starts with 00 instead of 10)
  // Send 0xc6 (11xxxxxx) 0x00 (00xxxxxx)
  uint8_t invalid_b2_prefix[] = {0xc6, 0x00};
  send(sv[1], invalid_b2_prefix, 2, 0);
  run_test("Invalid B2 prefix: recvFromClient returns false",
           !client2.recvFromClient(out));

  run_test("Invalid B2 prefix: client is disconnected", !client2.isConnected());

  run_test("Invalid B2 prefix: received 2 bytes",
           readExact(sv[1], err_resp, 2));
  run_test("Invalid B2 prefix: received encoded RESP_ERROR_HOST",
           err_resp[0] == 0xf0);
  run_test("Invalid B2 prefix: received encoded ERR_FRAMING",
           err_resp[1] == 0x80);

  close(sv[0]);
  close(sv[1]);
}

int main() {
  test_readonly_client();
  test_enhanced_client_protocol();
  test_enhanced_client_responses();
  test_enhanced_client_invalid_protocol();

  std::cout << "\nAll Client unit tests passed!" << std::endl;

  return 0;
}