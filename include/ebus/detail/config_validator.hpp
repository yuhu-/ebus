/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ebus/config.hpp"
#include "ebus/types.hpp"

namespace ebus::detail {

/**
 * Validates EbusConfig against protocol limits and logical constraints.
 */
class ConfigValidator {
 public:
  // Working Methods
  static bool validate(const EbusConfig& config);

  /**
   * Checks if two configs require a hardware restart (Bus/UART recreation).
   */
  static bool requiresHardwareRestart(const EbusConfig& old_cfg,
                                      const EbusConfig& new_cfg);
};

}  // namespace ebus::detail
