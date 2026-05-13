/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/socket.h>
#include <unistd.h>

#include <catch2/catch_all.hpp>
#include <chrono>
#include <ebus/detail/protocol_limits.hpp>
#include <ebus/types.hpp>
#include <vector>

#include "app/client.hpp"
#include "app/enhanced_protocol.hpp"
#include "core/request.hpp"
#include "test_utils.hpp"

using namespace ebus::detail;

TEST_CASE("ReadOnlyClient: capability checks", "[app][client][readonly]") {
  int sv[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  Request req;
  ReadOnlyClient client(sv[0], &req,
                        ebus::RuntimeConfig{}.network.outbound_buffer_size);

  REQUIRE(!client.isWriteCapable());
  REQUIRE(!client.wantsToSend());

  close(sv[0]);
  close(sv[1]);
}

TEST_CASE("EnhancedClient: Protocol basics", "[app][client][enhanced]") {
  int sv[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  Request req;
  EnhancedClient client(sv[0], &req,
                        ebus::RuntimeConfig{}.network.outbound_buffer_size);

  // Simple data byte (< 0x80)
  uint8_t out;
  uint8_t data = 0x15;
  send(sv[1], &data, 1, 0);
  REQUIRE(client.recvFromClient(out));
  REQUIRE(out == 0x15);

  // Enhanced escape sequence (CMD_SEND 0x01, value 0xaa)
  uint8_t escaped[2];
  ebus::detail::enhanced::Protocol::encode(0x01, 0xaa, escaped);
  send(sv[1], escaped, 2, 0);
  REQUIRE(client.recvFromClient(out));
  REQUIRE(out == 0xaa);

  // CMD_INIT should cause recvFromClient to return false and client to send
  // RESP_RESETTED
  uint8_t init_cmd[2];
  ebus::detail::enhanced::Protocol::encode(0x00, 0x00, init_cmd);
  send(sv[1], init_cmd, 2, 0);
  REQUIRE(!client.recvFromClient(out));

  uint8_t init_resp[2];
  REQUIRE(readExact(sv[1], init_resp, 2));
  REQUIRE(init_resp[0] == 0xc0);
  REQUIRE(init_resp[1] == 0x80);

  close(sv[0]);
  close(sv[1]);
}

TEST_CASE("EnhancedClient: Encoded responses mapping",
          "[app][client][enhanced][responses]") {
  int sv[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  Request req;
  req.setLockCounter(0);
  EnhancedClient client(sv[0], &req,
                        ebus::RuntimeConfig{}.network.outbound_buffer_size);

  // 1. Test: Arbitration Win
  if (req.busAvailable()) req.requestBus(0x33, true);
  req.busRequestCompleted();

  // Transparent Sniffer: Bridge client must see the SYN that opens the window
  req.run(ebus::Symbols::syn);
  client.onBusByte({ebus::Symbols::syn,
                    ebus::HandlerState::passive_receive_master, req.getState(),
                    req.getResult(), req.getLockCounter(), ebus::Clock::now()});

  req.run(0x33);  // Move FSM to firstWon

  client.onBusByte({0x33, ebus::HandlerState::passive_receive_master,
                    req.getState(), req.getResult(), req.getLockCounter(),
                    ebus::Clock::now()});

  uint8_t resp[2];
  REQUIRE(readExact(sv[1], resp, 2));
  REQUIRE(resp[0] == 0xc6);
  REQUIRE(resp[1] == 0xaa);

  REQUIRE(readExact(sv[1], resp, 2));
  REQUIRE(resp[0] == 0xc8);
  REQUIRE(resp[1] == 0xb3);

  // 2. Test: Short-form observation
  req.run(0x15);  // Move FSM to observeData
  client.onBusByte({0x15, ebus::HandlerState::passive_receive_master,
                    req.getState(), req.getResult(), req.getLockCounter(),
                    ebus::Clock::now()});

  uint8_t short_resp;
  REQUIRE(readExact(sv[1], &short_resp, 1));
  REQUIRE(short_resp == 0x15);

  // 3. Test: Long-form observation (>= 0x80) should be encoded
  req.run(0xaa);  // Move FSM to observeSyn
  client.onBusByte({0xaa, ebus::HandlerState::passive_receive_master,
                    req.getState(), req.getResult(), req.getLockCounter(),
                    ebus::Clock::now()});

  REQUIRE(readExact(sv[1], resp, 2));
  REQUIRE(resp[0] == 0xc6);
  REQUIRE(resp[1] == 0xaa);

  close(sv[0]);
  close(sv[1]);
}

TEST_CASE("EnhancedClient: Invalid protocol handling",
          "[app][client][enhanced][invalid]") {
  int sv[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  Request req;
  EnhancedClient client(sv[0], &req,
                        ebus::RuntimeConfig{}.network.outbound_buffer_size);

  uint8_t out;
  uint8_t err_resp[2];

  // Invalid first-byte prefix
  uint8_t invalid_b1_prefix[] = {0x80, 0xaa};
  send(sv[1], invalid_b1_prefix, 2, 0);
  REQUIRE(!client.recvFromClient(out));
  REQUIRE(!client.isConnected());
  REQUIRE(readExact(sv[1], err_resp, 2));
  REQUIRE(err_resp[0] == 0xf0);
  REQUIRE(err_resp[1] == 0x80);

  close(sv[0]);
  close(sv[1]);

  // Re-establish and test invalid second-byte prefix
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  EnhancedClient client2(sv[0], &req,
                         ebus::RuntimeConfig{}.network.outbound_buffer_size);

  uint8_t invalid_b2_prefix[] = {0xc6, 0x00};
  send(sv[1], invalid_b2_prefix, 2, 0);
  REQUIRE(!client2.recvFromClient(out));
  REQUIRE(!client2.isConnected());
  REQUIRE(readExact(sv[1], err_resp, 2));
  REQUIRE(err_resp[0] == 0xf0);
  REQUIRE(err_resp[1] == 0x80);

  close(sv[0]);
  close(sv[1]);
}
