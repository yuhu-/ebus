/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/protocol_math.hpp>
#include <ebus/sequence.hpp>
#include <ebus/utils.hpp>
#include <ebus/virtual_bus.hpp>

#include "utils/bus_simulator.hpp"

namespace ebus {

// Define the Impl struct here as an alias for BusSimulator
// This makes detail::BusSimulator a complete type when unique_ptr needs it.
struct VirtualBus::Impl : public detail::BusSimulator {
  explicit Impl(detail::platform::Bus& internal_bus)
      : detail::BusSimulator(internal_bus) {}
};

VirtualBus::VirtualBus(detail::platform::Bus& internal_bus)
    : impl_(std::make_unique<Impl>(internal_bus)) {}
VirtualBus::~VirtualBus() = default;

void VirtualBus::injectMasterMessage(uint8_t source, ebus::ByteView payload) {
  impl_->injectMasterMessage(source, makeSequence(payload));
}

void VirtualBus::addResponse(const AutoResponse& response) {
  impl_->addResponse(response);
}

void VirtualBus::addResponse(uint8_t source,
                             const std::string& masterPayloadHex,
                             const std::string& slavePayloadHex) {
  impl_->addResponse(source, masterPayloadHex, slavePayloadHex);
}

void VirtualBus::clear() { impl_->clear(); }

}  // namespace ebus
