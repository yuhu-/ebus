/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if EBUS_SIMULATION
#include <memory>
#include <vector>

#include "ebus/protocol_math.hpp"
#include "ebus/utils.hpp"

namespace ebus::detail::platform {
class BusSimulation;
}  // namespace ebus::detail::platform

namespace ebus {

namespace detail {
class BusSimulator;
}

class VirtualBus {
  friend class Controller;

 public:
  /**
   * @brief A MockReaction defines an automated action taken by the simulator
   * when a specific sequence of bytes is observed on the wire.
   */
  struct MockReaction {
    Sequence trigger;       ///< The sequence of bytes that triggers the action.
    Sequence action;        ///< The sequence of bytes to inject as a result.
    int repeat_count = 1;   ///< 0 for infinite, -1 for disabled, > 0 finite.
    uint32_t delay_ms = 0;  ///< Delay before injecting the action bytes.
  };

  ~VirtualBus();

  /**
   * @brief Constructs a VirtualBus instance tied to an internal simulation.
   */
  explicit VirtualBus(detail::platform::BusSimulation& internal_bus);

  /**
   * @brief Returns the underlying simulator engine for low-level test access.
   */
  detail::BusSimulator& getSimulator();

  /**
   * @brief Clears all configured reactions from the virtual bus.
   */
  void clear();

  /**
   * @brief Injects a master message onto the bus (QQ ZZ PB SB NN DBx CRC).
   * @param source The source address (QQ).
   * @param payload The master payload bytes (ZZ through DBx).
   */
  void injectMasterMessage(uint8_t source, ebus::ByteView payload);

  /**
   * @brief Adds a raw mock reaction to the simulator.
   */
  void addMockReaction(const MockReaction& reaction);

  /**
   * @brief Mocks a slave responding to a specific master request.
   * Triggers on: [Master Part (QQ ZZ PB SB NN DBx CRC)]
   * Injects: [ACK] [Slave Part (NN DBx CRC)]
   *
   * @param source The source address of the master message (QQ).
   * @param trigger_hex The master payload (ZZ PB SB NN DBx) in hex.
   * @param reaction_hex The slave payload (NN DBx) in hex.
   * @param repeat_count How many times this reaction should be triggered.
   * @param delay_ms Delay in milliseconds before mocking the slave.
   */
  void addSlaveReaction(uint8_t source, const std::string& trigger_hex,
                        const std::string& reaction_hex, int repeat_count = 1,
                        uint32_t delay_ms = 0);

  /**
   * @brief Mocks a receiver sending an ACK (0x00) for a master message.
   * Useful for Master-Master telegram simulations.
   * Triggers on: [Master Part]
   * Injects: [ACK]
   *
   * @param source The source address of the master message.
   * @param trigger_hex The master payload (ZZ PB SB NN DBx) in hex.
   * @param repeat_count How many times this reaction should be triggered.
   * @param delay_ms Delay in milliseconds before mocking the slave.
   */
  void addAckReaction(uint8_t source, const std::string& trigger_hex,
                      int repeat_count = 1, uint32_t delay_ms = 0);

  /**
   * @brief Mocks a receiver sending a NAK (0xff) for a master message.
   * Useful for simulating a busy slave or a slave detecting a CRC error in the
   * master part. Triggers on: [Master Part] Injects: [NAK]
   *
   * @param source The source address of the master message.
   * @param trigger_hex The master payload (ZZ PB SB NN DBx) in hex.
   * @param repeat_count How many times this reaction should be triggered.
   * @param delay_ms Delay in milliseconds before mocking the slave.
   */
  void addNakReaction(uint8_t source, const std::string& trigger_hex,
                      int repeat_count = 1, uint32_t delay_ms = 0);

  /**
   * @brief Mocks the master sending the final ACK (0x00) of a Master-Slave
   * telegram.
   * Triggers on: [Slave Part (NN DBx CRC)]
   * Injects: [ACK]
   *
   * @param trigger_hex The slave payload (NN DBx) in hex.
   * @param repeat_count How many times this reaction should be triggered.
   * @param delay_ms Delay in milliseconds before mocking the slave.
   */
  void addMasterAckReaction(const std::string& trigger_hex,
                            int repeat_count = 1, uint32_t delay_ms = 0);

  /**
   * @brief Mocks the master sending the final NAK (0xff) of a Master-Slave
   * telegram.
   * Useful for testing how a slave reacts when its response is rejected by the
   * master. Triggers on: [Slave Part (NN DBx CRC)] Injects: [NAK]
   *
   * @param trigger_hex The slave payload (NN DBx) in hex.
   * @param repeat_count How many times this reaction should be triggered.
   * @param delay_ms Delay in milliseconds before mocking the slave.
   */
  void addMasterNakReaction(const std::string& trigger_hex,
                            int repeat_count = 1, uint32_t delay_ms = 0);

  /**
   * @brief Adds a complex reaction that triggers on a combined Master+Slave
   * sequence. Useful for Broadcast listeners or special MS handshake mocking.
   *
   * Triggers on: [Master Part] [ACK] [Slave Part]
   * Injects: [Final Part (ACK or NAK)]
   *
   * @param source The source address of the master.
   * @param master_hex The master payload (ZZ PB SB NN DBx) in hex.
   * @param slave_hex The slave payload (NN DBx) in hex.
   * @param action_byte The final byte to inject (usually Symbols::ack).
   * @param repeat_count How many times this reaction should be triggered.
   * @param delay_ms Delay in milliseconds before mocking the slave.
   */
  void addFullTelegramReaction(uint8_t source, const std::string& master_hex,
                               const std::string& slave_hex,
                               uint8_t action_byte, int repeat_count = 1,
                               uint32_t delay_ms = 0);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ebus

#endif  // EBUS_SIMULATION