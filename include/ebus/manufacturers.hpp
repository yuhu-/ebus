/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace ebus {

/**
 * Returns the manufacturer name associated with the given eBUS ID.
 */
const char* manufacturer_name(const uint8_t& id);

}  // namespace ebus
