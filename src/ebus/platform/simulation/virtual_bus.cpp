/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#if EBUS_SIMULATION
#include <ebus/protocol_math.hpp>
#include <ebus/utils.hpp>
#include <ebus/virtual_bus.hpp>

#include "platform/simulation/bus_simulator.hpp"

namespace ebus {

// Define the Impl struct here as an alias for BusSimulator
// This makes detail::BusSimulator a complete type when unique_ptr needs it.
struct VirtualBus::Impl : public detail::BusSimulator {
  explicit Impl(detail::platform::BusSimulation& internal_bus)
      : detail::BusSimulator(internal_bus) {}
};

VirtualBus::VirtualBus(detail::platform::BusSimulation& internal_bus)
    : impl_(std::make_unique<Impl>(internal_bus)) {}
VirtualBus::~VirtualBus() = default;

detail::BusSimulator& VirtualBus::getSimulator() { return *impl_; }

void VirtualBus::clear() { impl_->clear(); }

void VirtualBus::injectMasterMessage(uint8_t source,
                                     const std::string& payload_hex) {
  uint8_t hex_buf[128];
  size_t hex_len = toBytes(payload_hex, hex_buf, sizeof(hex_buf));
  impl_->injectMasterMessage(source, ByteView(hex_buf, hex_len));
}

void VirtualBus::injectMasterMessage(uint8_t source, ebus::ByteView payload) {
  impl_->injectMasterMessage(source, payload);
}

void VirtualBus::injectMasterSlaveMessage(
    uint8_t source, const std::string& master_payload_hex,
    const std::string& slave_payload_hex) {
  uint8_t hex_buf1[128];
  uint8_t hex_buf2[128];
  size_t hex_len1 = toBytes(master_payload_hex, hex_buf1, sizeof(hex_buf1));
  size_t hex_len2 = toBytes(slave_payload_hex, hex_buf2, sizeof(hex_buf2));
  impl_->injectMasterSlaveMessage(source, ByteView(hex_buf1, hex_len1),
                                  ByteView(hex_buf2, hex_len2));
}

void VirtualBus::injectMasterSlaveMessage(uint8_t source,
                                          ebus::ByteView master_payload,
                                          ebus::ByteView slave_payload) {
  impl_->injectMasterSlaveMessage(source, master_payload, slave_payload);
}

uint32_t VirtualBus::addMockReaction(const MockReaction& reaction) {
  return impl_->addMockReaction(reaction);
}

void VirtualBus::removeMockReaction(uint32_t id) {
  impl_->removeMockReaction(id);
}

void VirtualBus::removeMockReaction(const Sequence& trigger) {
  impl_->removeMockReaction(trigger);
}

void VirtualBus::addSlaveReaction(uint8_t source,
                                  const std::string& trigger_hex,
                                  const std::string& reaction_hex,
                                  int repeat_count, uint32_t delay_ms) {
  uint8_t trig_buf[64];
  uint8_t react_buf[64];
  size_t trig_len = toBytes(trigger_hex, trig_buf, sizeof(trig_buf));
  size_t react_len = toBytes(reaction_hex, react_buf, sizeof(react_buf));
  ebus::Sequence slavePart = ebus::frameSlave(ByteView(react_buf, react_len));
  ebus::Sequence action;
  action.push_back(ebus::Symbols::ack, false);
  action.append(slavePart);

  MockReaction mock = {ebus::frameMaster(source, ByteView(trig_buf, trig_len)),
                       action, repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addAckReaction(uint8_t source, const std::string& trigger_hex,
                                int repeat_count, uint32_t delay_ms) {
  uint8_t hex_buf[64];
  size_t hex_len = toBytes(trigger_hex, hex_buf, sizeof(hex_buf));
  ebus::Sequence ack;
  ack.push_back(ebus::Symbols::ack, false);
  MockReaction mock = {ebus::frameMaster(source, ByteView(hex_buf, hex_len)),
                       ack, repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addNakReaction(uint8_t source, const std::string& trigger_hex,
                                int repeat_count, uint32_t delay_ms) {
  uint8_t hex_buf[64];
  size_t hex_len = toBytes(trigger_hex, hex_buf, sizeof(hex_buf));
  ebus::Sequence nak;
  nak.push_back(ebus::Symbols::nak, false);
  MockReaction mock = {ebus::frameMaster(source, ByteView(hex_buf, hex_len)),
                       nak, repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addMasterAckReaction(const std::string& trigger_hex,
                                      int repeat_count, uint32_t delay_ms) {
  uint8_t hex_buf[64];
  size_t hex_len = toBytes(trigger_hex, hex_buf, sizeof(hex_buf));
  ebus::Sequence ack;
  ack.push_back(ebus::Symbols::ack, false);
  ack.push_back(ebus::Symbols::syn, false);  // Master must send SYN after ACK
  // The trigger is the full framed slave part (NN DBx CRC)
  MockReaction mock = {ebus::frameSlave(ByteView(hex_buf, hex_len)), ack,
                       repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addMasterNakReaction(const std::string& trigger_hex,
                                      int repeat_count, uint32_t delay_ms) {
  uint8_t hex_buf[64];
  size_t hex_len = toBytes(trigger_hex, hex_buf, sizeof(hex_buf));
  ebus::Sequence nak;
  nak.push_back(ebus::Symbols::nak, false);
  // The trigger is the full framed slave part (NN DBx CRC)
  MockReaction mock = {ebus::frameSlave(ByteView(hex_buf, hex_len)), nak,
                       repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addFullTelegramReaction(uint8_t source,
                                         const std::string& master_hex,
                                         const std::string& slave_hex,
                                         uint8_t action_byte, int repeat_count,
                                         uint32_t delay_ms) {
  uint8_t master_buf[64];
  uint8_t slave_buf[64];
  size_t master_len = toBytes(master_hex, master_buf, sizeof(master_buf));
  size_t slave_len = toBytes(slave_hex, slave_buf, sizeof(slave_buf));
  ebus::Sequence masterPart =
      ebus::frameMaster(source, ByteView(master_buf, master_len));
  ebus::Sequence slavePart = ebus::frameSlave(ByteView(slave_buf, slave_len));

  ebus::Sequence trigger = masterPart;
  trigger.push_back(ebus::Symbols::ack, false);
  trigger.append(slavePart);
  trigger.extend();

  ebus::Sequence action;
  action.push_back(action_byte, false);

  // If the simulated master is sending an ACK to conclude the telegram,
  // it must release the bus with a SYN symbol.
  if (action_byte == ebus::Symbols::ack) {
    action.push_back(ebus::Symbols::syn, false);
  }

  MockReaction mock = {trigger, action, repeat_count, delay_ms};
  addMockReaction(mock);
}

}  // namespace ebus

#endif  // EBUS_SIMULATION
