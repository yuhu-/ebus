/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if defined(EBUS_SIMULATION)
#include <memory>
#include <vector>

#include "ebus/protocol_math.hpp"
#include "ebus/utils.hpp"

namespace ebus::detail::platform {
class BusSimulation;
}  // namespace ebus::detail::platform

namespace ebus {

class VirtualBus {
  friend class Controller;

 public:
  ~VirtualBus();

  /**
   * @brief Injects a master message onto the bus with proper framing and CRC.
   * @param source The source address of the master message.
   * @param payload The raw payload bytes of the master message (excluding CRC).
   */
  void injectMasterMessage(uint8_t source, ebus::ByteView payload);

  struct AutoResponse {
    std::vector<uint8_t> trigger_pattern;
    std::vector<uint8_t> response_data;
    int repeat_count = 1;  // 0 for infinite, -1 for disabled
  };

  /**
   * @brief Adds an automatic response that triggers when a specific pattern is
   * observed on the bus.
   * @param response The AutoResponse configuration defining the trigger
   * pattern, response data, delay, and repeat count.
   */
  void addResponse(const AutoResponse& response);

  /**
   * @brief Adds a master-slave response pair that triggers when a specific
   * master message is observed. After an optional delay, the corresponding
   * slave response is injected.
   * @param source The expected source address of the master message.
   * @param masterPayloadHex The expected master payload in hexadecimal string
   * format (without CRC).
   * @param slavePayloadHex The slave response payload in hexadecimal string
   * format (without ACK or CRC).
   */
  void addResponse(uint8_t source, const std::string& masterPayloadHex,
                   const std::string& slavePayloadHex);

  /**
   * @brief Clears all configured responses from the virtual bus.
   */
  void clear();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  explicit VirtualBus(detail::platform::BusSimulation& internal_bus);
};

}  // namespace ebus

#endif  // EBUS_SIMULATION