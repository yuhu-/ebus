/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/**
 * Convenience header for the eBUS library.
 * Includes all public components required for building eBUS applications.
 */

#include "ebus/address.hpp"
#include "ebus/byte_view.hpp"
#include "ebus/callbacks.hpp"
#include "ebus/config.hpp"
#include "ebus/controller.hpp"
#include "ebus/data_types.hpp"
#include "ebus/device.hpp"
#include "ebus/metrics.hpp"
#include "ebus/protocol_math.hpp"
#include "ebus/sequence.hpp"
#include "ebus/status.hpp"
#include "ebus/types.hpp"
#include "ebus/utils.hpp"
#if defined(EBUS_SIMULATION)
#include "ebus/virtual_bus.hpp"
#endif