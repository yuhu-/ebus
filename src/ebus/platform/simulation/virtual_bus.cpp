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
  impl_->injectMasterMessage(source, ebus::toVector(payload_hex));
}

void VirtualBus::addMockReaction(const MockReaction& reaction) {
  impl_->addMockReaction({reaction.trigger, reaction.action,
                          reaction.repeat_count, reaction.delay_ms});
}

void VirtualBus::addSlaveReaction(uint8_t source,
                                  const std::string& trigger_hex,
                                  const std::string& reaction_hex,
                                  int repeat_count, uint32_t delay_ms) {
  ebus::Sequence slavePart = ebus::frameSlave(ebus::toVector(reaction_hex));
  ebus::Sequence action;
  action.pushBack(ebus::Symbols::ack, false);
  action.append(slavePart);

  MockReaction mock = {ebus::frameMaster(source, ebus::toVector(trigger_hex)),
                       action, repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addAckReaction(uint8_t source, const std::string& trigger_hex,
                                int repeat_count, uint32_t delay_ms) {
  ebus::Sequence ack;
  ack.pushBack(ebus::Symbols::ack, false);
  MockReaction mock = {ebus::frameMaster(source, ebus::toVector(trigger_hex)),
                       ack, repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addNakReaction(uint8_t source, const std::string& trigger_hex,
                                int repeat_count, uint32_t delay_ms) {
  ebus::Sequence nak;
  nak.pushBack(ebus::Symbols::nak, false);
  MockReaction mock = {ebus::frameMaster(source, ebus::toVector(trigger_hex)),
                       nak, repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addMasterAckReaction(const std::string& trigger_hex,
                                      int repeat_count, uint32_t delay_ms) {
  ebus::Sequence ack;
  ack.pushBack(ebus::Symbols::ack, false);
  ack.pushBack(ebus::Symbols::syn, false);  // Master must send SYN after ACK
  // The trigger is the full framed slave part (NN DBx CRC)
  MockReaction mock = {ebus::frameSlave(ebus::toVector(trigger_hex)), ack,
                       repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addMasterNakReaction(const std::string& trigger_hex,
                                      int repeat_count, uint32_t delay_ms) {
  ebus::Sequence nak;
  nak.pushBack(ebus::Symbols::nak, false);
  // The trigger is the full framed slave part (NN DBx CRC)
  MockReaction mock = {ebus::frameSlave(ebus::toVector(trigger_hex)), nak,
                       repeat_count, delay_ms};
  addMockReaction(mock);
}

void VirtualBus::addFullTelegramReaction(uint8_t source,
                                         const std::string& master_hex,
                                         const std::string& slave_hex,
                                         uint8_t action_byte, int repeat_count,
                                         uint32_t delay_ms) {
  ebus::Sequence masterPart =
      ebus::frameMaster(source, ebus::toVector(master_hex));
  ebus::Sequence slavePart = ebus::frameSlave(ebus::toVector(slave_hex));

  ebus::Sequence trigger = masterPart;
  trigger.pushBack(ebus::Symbols::ack, false);
  trigger.append(slavePart);
  trigger.extend();

  ebus::Sequence action;
  action.pushBack(action_byte, false);

  // If the simulated master is sending an ACK to conclude the telegram,
  // it must release the bus with a SYN symbol.
  if (action_byte == ebus::Symbols::ack) {
    action.pushBack(ebus::Symbols::syn, false);
  }

  MockReaction mock = {trigger, action, repeat_count, delay_ms};
  addMockReaction(mock);
}

}  // namespace ebus

#endif  // EBUS_SIMULATION
