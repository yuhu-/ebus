/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <ebus/manufacturers.hpp>

namespace ebus {

static constexpr const char* kManufacturerTable[256] = {
    nullptr,         nullptr,         nullptr,      "Junkers",   "Bosch",
    "Buderus",       "Dungs",         nullptr,      "Siemens",   nullptr,
    nullptr,         "Danfoss",       nullptr,      nullptr,     nullptr,
    "FH Ostfalia",   "TEM",           "Lamberti",   nullptr,     nullptr,
    "CEB",           "Landis-Staefa", "FERRO",      "MONDIAL",   "Wikon",
    "Wolf",          "Elco",          nullptr,      nullptr,     nullptr,
    nullptr,         "RAWE",          nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "Satronic",  nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     "ENCON",
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Paradigma",     "Ochsner",       nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Kromschroeder", nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         "Eberle",        nullptr,      nullptr,     nullptr,
    nullptr,         "EBV",           nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "Graesslin", nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "Gira",      nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     "ebm-papst",
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      "SIG",       nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Theben",        nullptr,         "Thermowatt", nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    "Vaillant",      nullptr,         "Protherm",   nullptr,     nullptr,
    nullptr,         "Saunier Duval", nullptr,      nullptr,     nullptr,
    nullptr,         "Toby",          nullptr,      nullptr,     nullptr,
    nullptr,         "Weishaupt",     nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         "Honeywell",  nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         "Wolf",          nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         "danman.eu",  nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         nullptr,         nullptr,      nullptr,     nullptr,
    nullptr,         "ebusd.eu",      nullptr,      nullptr};

const char* manufacturerName(uint8_t id) {
  const char* name = kManufacturerTable[id];
  return name ? name : "Unknown";
}

}  // namespace ebus
