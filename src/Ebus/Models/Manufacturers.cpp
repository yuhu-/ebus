/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/Manufacturers.hpp>
#include <map>

namespace ebus {

static const std::map<uint8_t, const char*> ManufacturerNames = {
    {0x03, "Junkers"},       {0x04, "Bosch"},         {0x05, "Buderus"},
    {0x06, "Dungs"},         {0x08, "Siemens"},       {0x0b, "Danfoss"},
    {0x0f, "FH Ostfalia"},   {0x10, "TEM"},           {0x11, "Lamberti"},
    {0x14, "CEB"},           {0x15, "Landis-Staefa"}, {0x16, "FERRO"},
    {0x17, "MONDIAL"},       {0x18, "Wikon"},         {0x19, "Wolf"},
    {0x1a, "Elco"},          {0x20, "RAWE"},          {0x30, "Satronic"},
    {0x40, "ENCON"},         {0x46, "Paradigma"},     {0x47, "Ochsner"},
    {0x50, "Kromschroeder"}, {0x60, "Eberle"},        {0x65, "EBV"},
    {0x75, "Graesslin"},     {0x7f, "Gira"},          {0x85, "ebm-papst"},
    {0x95, "SIG"},           {0xa5, "Theben"},        {0xa7, "Thermowatt"},
    {0xb5, "Vaillant"},      {0xb7, "Protherm"},      {0xbb, "Saunier Duval"},
    {0xc0, "Toby"},          {0xc5, "Weishaupt"},     {0xcc, "Honeywell"},
    {0xd4, "Wolf"},          {0xda, "danman.eu"},     {0xfd, "ebusd.eu"}};

const char* manufacturer_name(const uint8_t& id) {
  auto it = ManufacturerNames.find(id);
  if (it != ManufacturerNames.end()) return it->second;
  return "Unknown";
}

}  // namespace ebus
