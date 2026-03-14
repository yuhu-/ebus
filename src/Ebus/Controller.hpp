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

// Top-level controller that wires Bus, Request, Handler and ByteRunner.
// Owns lifecycle so other parts of the system can use a single entry point.

#pragma once

#include <memory>

#include "Bus.hpp"
#include "BusHandler.hpp"
#include "Handler.hpp"
#include "Queue.hpp"
#include "Request.hpp"

namespace ebus {

struct ebusConfig {
  uint8_t address = 0xff;  // default address is 0xff
  uint16_t window = 4300;  // usually between 4300-4456 us
  uint16_t offset = 80;    // mainly for context switch and write
  busConfig bus = {};
};

class Controller {
 public:
  Controller() = default;
  explicit Controller(const ebusConfig& config);
  ~Controller() = default;

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  void configure(const ebusConfig& config);

  void start();
  void stop();

  void setAddress(const uint8_t& address);
  void setWindow(const uint16_t& window);
  void setOffset(const uint16_t& offset);

  Request* getRequest();
  const Request* getRequest() const;

  Bus* getBus();
  const Bus* getBus() const;

  BusHandler* getBusHandler();
  const BusHandler* getBusHandler() const;

  Handler* getHandler();
  const Handler* getHandler() const;

  bool isConfigured() const noexcept;
  bool isRunning() const noexcept;

 private:
  ebusConfig config_;

  std::unique_ptr<Request> request_;
  std::unique_ptr<Bus> bus_;
  std::unique_ptr<BusHandler> busHandler_;
  std::unique_ptr<Handler> handler_;

  bool configured_ = false;
  bool running_ = false;

  void constructMembers();
};

}  // namespace ebus