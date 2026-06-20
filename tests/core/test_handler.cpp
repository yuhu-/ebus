/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <catch2/catch_all.hpp>
#include <cstddef>
#include <ebus/data_types.hpp>
#include <ebus/metrics.hpp>
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>
#include <iomanip>
#include <iostream>
#include <string>

#include "core/bus_monitor.hpp"
#include "core/handler.hpp"
#include "core/request.hpp"
#include "core/telegram.hpp"
#include "platform/bus.hpp"
#include "test_helpers.hpp"

using namespace ebus::detail;

struct TestCase {
  ebus::MessageType message_type;
  uint8_t address;
  std::string description;
  std::string read_string;
  std::string send_string = "";
  struct ExpectedResult {
    int telegram;
    int errors;
  } expected;
};

SCENARIO("Handler processes eBUS messages correctly", "[core][handler]") {
  GIVEN("A set of eBUS test cases") {
    // clang-format off
    std::vector<TestCase> test_cases = {
        {ebus::MessageType::passive, 0x33, "passive MS: Normal", "ff52b509030d0600430003b0fba901d000", "", {1, 0}},
        {ebus::MessageType::passive, 0x33, "passive MS: Master CRC error -> NAK -> Retry OK", "ff52b509030d060044ffff52b509030d0600430003b0fba901d000", "", {1, 0}},
        {ebus::MessageType::passive, 0x33, "passive MS: Master valid -> NAK -> Retry OK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d000", "", {1, 0}},
        {ebus::MessageType::passive, 0x33, "passive MS: Master valid -> NAK -> Retry valid -> NAK", "ff52b509030d060043ffff52b509030d060043ff", "", {0, 1}},
        {ebus::MessageType::passive, 0x33, "passive MS: Slave CRC error -> NAK -> Retry OK", "ff52b509030d0600430003b0fba902d0ff03b0fba901d000", "", {1, 1}},
        {ebus::MessageType::passive, 0x33, "passive MS: Slave valid -> NAK -> Retry valid -> NAK", "ff52b509030d0600430003b0fba901d0ff03b0fba901d0ff", "", {0, 1}},
        {ebus::MessageType::passive, 0x33, "passive MS: Master NAK/Retry OK - Slave NAK/Retry OK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d000", "", {1, 0}},
        {ebus::MessageType::passive, 0x33, "passive MS: Master NAK/Retry OK - Slave NAK/Retry NAK", "ff52b509030d060043ffff52b509030d0600430003b0fba901d0ff03b0fba901d0ff", "", {0, 1}},
        {ebus::MessageType::passive, 0x33, "passive MM: Normal", "1000b5050427002400d900", "", {1, 0}},
        {ebus::MessageType::passive, 0x33, "passive BC: Malformed sequence", "00fe0704003c", "", {0, 1}},
        {ebus::MessageType::passive, 0x33, "passive 00: Reset", "00", "", {0, 1}},
        {ebus::MessageType::passive, 0x33, "passive 0704: Scan", "002e0704004e", "", {0, 1}},
        {ebus::MessageType::passive, 0x33, "passive BC: Normal", "10fe07000970160443183105052592", "", {1, 0}},
        {ebus::MessageType::passive, 0x33, "passive MS: Slave CRC 0x21 is missing", "1008b5130304cd017f000acd01000000000100010000", "", {0, 2}},

        {ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/ACK", "0038070400ab000ab5504d5330300107430245ff0ab5504d533030010743024600", "", {1, 0}},
        {ebus::MessageType::reactive, 0x33, "reactive MS: Slave NAK/NAK", "0038070400ab000ab5504d5330300107430245ff0ab5504d5330300107430245ff", "", {0, 1}},
        {ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/correct", "0038070400acff0038070400ab000ab5504d533030010743024600", "", {1, 1}},
        {ebus::MessageType::reactive, 0x33, "reactive MS: Master defect/defect", "0038070400ff0038070400acff", "", {0, 1}}, // 2->1 because of Plausibility Filtering
        {ebus::MessageType::reactive, 0x33, "reactive MS: Slave defect (callback)", "003807050030", "", {0, 1}},
        {ebus::MessageType::reactive, 0x33, "reactive MM: Normal", "003307040014", "", {1, 0}},
        {ebus::MessageType::reactive, 0x33, "reactive BC: Normal", "00fe0704003b", "", {1, 0}},

        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Normal", "33feb5050427002d002c", "feb5050427002d00", {1, 0}},
        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost", "01feb5050427002d007b", "feb505042700cc00", {1, 0}},
        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority lost/wrong byte", "01ab", "feb505042700cc00", {0, 1}},
        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/won", "13aa33feb505042700cc0010", "feb505042700cc00", {1, 0}},
        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority fit/lost", "13aa13feb5050427002d0088", "feb505042700cc00", {1, 0}},
        {ebus::MessageType::active, 0x33, "active BC: Request Bus - Priority retry/error", "13a0", "feb505042700cc00", {0, 0}},
        {ebus::MessageType::active, 0x33, "active MS: Normal", "3352b509030d46003600013fa400", "52b509030d4600", {1, 0}},
        {ebus::MessageType::active, 0x33, "active MS: Master valid -> NAK -> Retry OK - Slave CRC error -> NAK -> Retry OK", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa400", "52b509030d4600", {1, 1}},
        {ebus::MessageType::active, 0x33, "active MS: Master valid -> NAK -> Retry OK - Slave CRC error -> NAK -> Retry NAK", "3352b509030d460036ff3352b509030d46003600013fa3ff013fa3ff", "52b509030d4600", {0, 3}},
        {ebus::MessageType::active, 0x33, "active MS: Master valid -> NAK -> Retry NAK", "3352b509030d460036ff3352b509030d460036ff", "52b509030d4600", {0, 1}},
        {ebus::MessageType::active, 0x33, "active MM: Master valid -> NAK -> Retry ACK", "3310b57900fbff3310b57900fb00", "10b57900", {1, 0}},
    };
    // clang-format on

    for (const auto& tc : test_cases) {
      WHEN(tc.description) {
        ebus::BusConfig config;
        ebus::RuntimeConfig runtime = {.address = 0x33};
        Request request;
        BusMonitor monitor;
        platform::Bus bus(config, runtime, &request, &monitor);
        Handler handler(runtime.address, &bus, &request, &monitor);

        int telegram_count = 0;
        int error_count = 0;

        struct TestCtx {
          int& tc;
          int& ec;
          void onWon() { INFO("request: won"); }
          void onLost() { INFO("request: lost"); }
          void onReactive(const ebus::ReactiveInfo& info) {
            if (ebus::contains(info.master_view, {0x07, 0x04}))
              info.slave_response.assign(
                  ebus::toVector("0ab5504d53303001074302"));
            if (ebus::contains(info.master_view, {0x07, 0x05}))
              info.slave_response.assign(
                  ebus::toVector("0ab5504d533030010743"));
          }
          void onProtocol(const ebus::ProtocolInfo& info) {
            if (info.is_error) {
              ec++;
              INFO("error: " << ebus::toString(info.protocol_error));
            } else {
              tc++;
              INFO("telegram received");
            }
          }
          void onWrite(const uint8_t& b) {
            INFO("<- write: " << ebus::toString(b));
          }
        } ctx{telegram_count, error_count};

        handler.setBusRequestWonCallback(
            BusRequestWonCallback::bind<TestCtx, &TestCtx::onWon>(&ctx));
        handler.setBusRequestLostCallback(
            BusRequestLostCallback::bind<TestCtx, &TestCtx::onLost>(&ctx));
        handler.setReactiveCallback(
            HandlerReactiveCallback::bind<TestCtx, &TestCtx::onReactive>(&ctx));
        handler.setProtocolCallback(
            HandlerProtocolCallback::bind<TestCtx, &TestCtx::onProtocol>(&ctx));

        bus.addWriteListener(
            platform::BusBase::WriteListener::bind<TestCtx, &TestCtx::onWrite>(
                &ctx));

        handler.setSourceAddress(tc.address);

        ebus::Sequence seq;
        seq.assign(
            ebus::toVector(std::string("aaaaaa") + tc.read_string + "aaaaaa"));

        if (tc.send_string.size() > 0) {
          handler.sendActiveMessage(ebus::toVector(tc.send_string));
        }

        bool busRequestFlag = false;
        for (size_t i = 0; i < seq.size(); i++) {
          switch (handler.getState()) {
            case ebus::HandlerState::release_bus:
              i--;
              break;
            default:
              INFO("->  read: " << ebus::toString(seq[i]));
              break;
          }

          if (busRequestFlag) {
            request.busRequestCompleted();
            busRequestFlag = false;
          }
          request.run(seq[i]);
          handler.run({seq[i], handler.getState(), request.getState(),
                       request.getResult(), request.getLockCounter(),
                       ebus::Clock::now()});

          if (seq[i] == ebus::Symbols::syn && request.busRequestPending()) {
            INFO("ISR - write address");
            bus.writeByte(request.busRequestAddress());
            busRequestFlag = true;
          }
        }

        THEN("The expected number of telegrams and errors are reported") {
          REQUIRE(telegram_count == tc.expected.telegram);
          REQUIRE(error_count == tc.expected.errors);
        }
      }
    }
  }
}
