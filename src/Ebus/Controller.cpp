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

ebus::Controller::Controller(const ebusConfig& config) : config_(config) {
  configured_ = true;
  constructMembers();
}

void ebus::Controller::configure(const ebusConfig& config) {
  if (running_) return;
  config_ = config;
  configured_ = true;
  constructMembers();
}

void ebus::Controller::start() {
  if (!configured_) return;
  if (running_) return;
  bus_->start();
  busHandler_->start();
  running_ = true;
}

void ebus::Controller::stop() {
  if (!configured_) return;
  if (!running_) return;
  busHandler_->stop();
  bus_->stop();
  running_ = false;
}

void ebus::Controller::setAddress(const uint8_t& address) {
  if (!configured_) {
    config_.address = address;
    return;
  }
  config_.address = address;
  handler_->setSourceAddress(address);
}

void ebus::Controller::setWindow(const uint16_t& window) {
  if (!configured_) {
    config_.window = window;
    return;
  }
  config_.window = window;
  bus_->setWindow(window);
}

void ebus::Controller::setOffset(const uint16_t& offset) {
  if (!configured_) {
    config_.offset = offset;
    return;
  }
  config_.offset = offset;
  bus_->setOffset(offset);
}

ebus::Request* ebus::Controller::getRequest() {
  return configured_ ? request_.get() : nullptr;
}

const ebus::Request* ebus::Controller::getRequest() const {
  return configured_ ? request_.get() : nullptr;
}

ebus::Bus* ebus::Controller::getBus() {
  return configured_ ? bus_.get() : nullptr;
}

const ebus::Bus* ebus::Controller::getBus() const {
  return configured_ ? bus_.get() : nullptr;
}

ebus::BusHandler* ebus::Controller::getBusHandler() {
  return configured_ ? busHandler_.get() : nullptr;
}

const ebus::BusHandler* ebus::Controller::getBusHandler() const {
  return configured_ ? busHandler_.get() : nullptr;
}

ebus::Handler* ebus::Controller::getHandler() {
  return configured_ ? handler_.get() : nullptr;
}

const ebus::Handler* ebus::Controller::getHandler() const {
  return configured_ ? handler_.get() : nullptr;
}

bool ebus::Controller::isConfigured() const noexcept { return configured_; }

bool ebus::Controller::isRunning() const noexcept { return running_; }

void ebus::Controller::constructMembers() {
  request_.reset(new Request());
  bus_.reset(new Bus(config_.bus, request_.get()));
  handler_.reset(new Handler(config_.address, bus_.get(), request_.get()));
  busHandler_.reset(
      new BusHandler(request_.get(), handler_.get(), bus_.get()->getQueue()));

  bus_.get()->setWindow(config_.window);
  bus_.get()->setOffset(config_.offset);
}
