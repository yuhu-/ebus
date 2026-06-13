/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ebus/config.hpp>
#include <ebus/detail/json_reader.hpp>
#include <ebus/detail/json_writer.hpp>
#include <ebus/utils.hpp>
#include <string_view>

namespace ebus {

void RuntimeConfig::reset() { *this = RuntimeConfig{}; }

void RuntimeConfig::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();

  writer.writeField("address", address);
  writer.writeField("lock_counter", lock_counter);
  writer.writeField("system_inquiry", system_inquiry);
  writer.writeField("system_response", system_response);

  {
    auto busScope = writer.objectScope("bus");
    writer.writeField("window_us", bus.window_us);
    writer.writeField("offset_us", bus.offset_us);
    writer.writeField("watchdog_timeout_ms", bus.watchdog_timeout_ms);
    writer.writeField("syn_gen", bus.syn_gen);
  }

  {
    auto diagScope = writer.objectScope("diagnostics");
    writer.writeField("level", toString(diagnostics.level));
    writer.writeField("log_size", diagnostics.log_size);
  }

  {
    auto netScope = writer.objectScope("network");
    writer.writeField("session_timeout_ms", network.session_timeout_ms);
    writer.writeField("transmit_timeout_ms", network.transmit_timeout_ms);
    writer.writeField("outbound_buffer_size", network.outbound_buffer_size);
  }

  {
    auto devScope = writer.objectScope("device");
    writer.writeField("scan_on_startup", device.scan_on_startup);
    writer.writeField("initial_delay_s", device.initial_delay_s);
    writer.writeField("startup_interval_s", device.startup_interval_s);
    writer.writeField("max_startup_scans", device.max_startup_scans);
  }

  {
    auto schedScope = writer.objectScope("scheduler");
    writer.writeField("max_send_attempts", scheduler.max_send_attempts);
    writer.writeField("base_backoff_ms", scheduler.base_backoff_ms);
    writer.writeField("fsm_timeout_ms", scheduler.fsm_timeout_ms);
    writer.writeField("total_timeout_ms", scheduler.total_timeout_ms);
  }
}

RuntimeConfig RuntimeConfig::fromJson(std::string_view json) {
  RuntimeConfig cfg;
  cfg.mergeFromJson(json);
  return cfg;
}

bool RuntimeConfig::mergeFromJson(std::string_view json) {
  detail::JsonReader reader(json);
  if (reader.next() != detail::JsonReader::Token::object_start) return false;

  reader.forEachField([&](std::string_view key, detail::JsonReader& r) {
    if (key == "address") {
      r.next();
      auto val = r.asNumStrict<int>();
      if (val) address = static_cast<uint8_t>(*val);
      return val.has_value();
    }
    if (key == "lock_counter") {
      r.next();
      auto val = r.asNumStrict<int>();
      if (val) lock_counter = static_cast<uint8_t>(*val);
      return val.has_value();
    }
    if (key == "system_inquiry") {
      r.next();
      system_inquiry = r.asBool();
      return true;
    }
    if (key == "system_response") {
      r.next();
      system_response = r.asBool();
      return true;
    }
    if (key == "bus") {
      if (r.next() == detail::JsonReader::Token::object_start) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "window_us") {
            inner.next();
            auto val = inner.asNumStrict<uint16_t>();
            if (val) bus.window_us = *val;
            return val.has_value();
          }
          if (k == "offset_us") {
            inner.next();
            auto val = inner.asNumStrict<uint16_t>();
            if (val) bus.offset_us = *val;
            return val.has_value();
          }
          if (k == "watchdog_timeout_ms") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) bus.watchdog_timeout_ms = *val;
            return val.has_value();
          }
          if (k == "syn_gen") {
            inner.next();
            bus.syn_gen = inner.asBool();
            return true;
          }
          return false;
        });
      }
      return true;
    }
    if (key == "diagnostics") {
      if (r.next() == detail::JsonReader::Token::object_start) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "level") {
            auto token = inner.next();
            if (token == detail::JsonReader::Token::number) {
              auto val = inner.asNumStrict<int>();
              if (val) diagnostics.level = static_cast<LogLevel>(*val);
              return val.has_value();
            } else if (token == detail::JsonReader::Token::string) {
              std::string_view lv = inner.value();
              if (lv == "none")
                diagnostics.level = LogLevel::none;
              else if (lv == "error")
                diagnostics.level = LogLevel::error;
              else if (lv == "info")
                diagnostics.level = LogLevel::info;
              else if (lv == "debug")
                diagnostics.level = LogLevel::debug;
              else
                return false;
              return true;
            }
            return false;
          }
          if (k == "log_size") {
            inner.next();
            auto val = inner.asNumStrict<size_t>();
            if (val) diagnostics.log_size = *val;
            return val.has_value();
          }
          return false;
        });
      }
      return true;
    }
    if (key == "network") {
      if (r.next() == detail::JsonReader::Token::object_start) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "session_timeout_ms") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) network.session_timeout_ms = *val;
            return val.has_value();
          }
          if (k == "transmit_timeout_ms") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) network.transmit_timeout_ms = *val;
            return val.has_value();
          }
          if (k == "outbound_buffer_size") {
            inner.next();
            auto val = inner.asNumStrict<size_t>();
            if (val) network.outbound_buffer_size = *val;
            return val.has_value();
          }
          return false;
        });
      }
      return true;
    }
    if (key == "device") {
      if (r.next() == detail::JsonReader::Token::object_start) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "scan_on_startup") {
            inner.next();
            device.scan_on_startup = inner.asBool();
            return true;
          }
          if (k == "initial_delay_s") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) device.initial_delay_s = *val;
            return val.has_value();
          }
          if (k == "startup_interval_s") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) device.startup_interval_s = *val;
            return val.has_value();
          }
          if (k == "max_startup_scans") {
            inner.next();
            auto val = inner.asNumStrict<int>();
            if (val) device.max_startup_scans = static_cast<uint8_t>(*val);
            return val.has_value();
          }
          return false;
        });
      }
      return true;
    }
    if (key == "scheduler") {
      if (r.next() == detail::JsonReader::Token::object_start) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "max_send_attempts") {
            inner.next();
            auto val = inner.asNumStrict<int>();
            if (val) scheduler.max_send_attempts = static_cast<uint8_t>(*val);
            return val.has_value();
          }
          if (k == "base_backoff_ms") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) scheduler.base_backoff_ms = *val;
            return val.has_value();
          }
          if (k == "fsm_timeout_ms") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) scheduler.fsm_timeout_ms = *val;
            return val.has_value();
          }
          if (k == "total_timeout_ms") {
            inner.next();
            auto val = inner.asNumStrict<uint32_t>();
            if (val) scheduler.total_timeout_ms = *val;
            return val.has_value();
          }
          return false;
        });
      }
      return true;
    }
    return false;
  });

  return true;
}

bool RuntimeConfig::isValidJson(std::string_view json) {
  return detail::JsonReader::validate(json);
}

void BusConfig::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();

#if defined(ESP_PLATFORM) && !EBUS_SIMULATION
  writer.writeField("platform", "esp32");
  writer.writeField("uart_port", uart_port);
  writer.writeField("rx_pin", rx_pin);
  writer.writeField("tx_pin", tx_pin);
  writer.writeField("timer_group", timer_group);
  writer.writeField("timer_idx", timer_idx);
#elif defined(POSIX) && !EBUS_SIMULATION
  writer.writeField("platform", "posix");
  writer.writeField("device", device);
#endif
}

void EbusConfig::toJson(detail::JsonWriter& writer) const {
  auto scope = writer.objectScope();
  writer.writeField("runtime", runtime);
  writer.writeField("bus_hardware", bus);
}

}  // namespace ebus