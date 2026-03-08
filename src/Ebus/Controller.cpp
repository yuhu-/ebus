/*
 * Copyright (C) 2026 Roland Jax
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

#include "Controller.hpp"

#include <stdexcept>

ebus::Controller::Controller(const ebusConfig& cfg) : config(cfg) {
  configured = true;
  constructMembers();
}

void ebus::Controller::configure(const ebusConfig& cfg) {
  if (running) throw std::runtime_error("Cannot configure while running");
  config = cfg;
  configured = true;
  constructMembers();
}

void ebus::Controller::start() {
  if (!configured) return;
  if (running) return;
  bus->start();
  busHandler->start();
  running = true;
}

void ebus::Controller::stop() {
  if (!configured) return;
  if (!running) return;
  busHandler->stop();
  bus->stop();
  running = false;
}

void ebus::Controller::setAddress(const uint8_t& address) {
  if (!configured) {
    config.address = address;
    return;
  }
  config.address = address;
  handler->setSourceAddress(address);
}

void ebus::Controller::setWindow(const uint16_t& window) {
  if (!configured) {
    config.window = window;
    return;
  }
  config.window = window;
  bus->setWindow(window);
}

void ebus::Controller::setOffset(const uint16_t& offset) {
  if (!configured) {
    config.offset = offset;
    return;
  }
  config.offset = offset;
  bus->setOffset(offset);
}

ebus::Request* ebus::Controller::getRequest() {
  return configured ? request.get() : nullptr;
}

const ebus::Request* ebus::Controller::getRequest() const {
  return configured ? request.get() : nullptr;
}

ebus::Bus* ebus::Controller::getBus() {
  return configured ? bus.get() : nullptr;
}

const ebus::Bus* ebus::Controller::getBus() const {
  return configured ? bus.get() : nullptr;
}

ebus::BusHandler* ebus::Controller::getBusHandler() {
  return configured ? busHandler.get() : nullptr;
}

const ebus::BusHandler* ebus::Controller::getBusHandler() const {
  return configured ? busHandler.get() : nullptr;
}

ebus::Handler* ebus::Controller::getHandler() {
  return configured ? handler.get() : nullptr;
}

const ebus::Handler* ebus::Controller::getHandler() const {
  return configured ? handler.get() : nullptr;
}

bool ebus::Controller::isConfigured() const noexcept { return configured; }

bool ebus::Controller::isRunning() const noexcept { return running; }

void ebus::Controller::constructMembers() {
  request.reset(new Request());
  bus.reset(new Bus(config.bus, request.get()));
  handler.reset(new Handler(config.address, bus.get(), request.get()));
  busHandler.reset(
      new BusHandler(request.get(), handler.get(), bus->getQueue()));

  bus->setWindow(config.window);
  bus->setOffset(config.offset);
}
