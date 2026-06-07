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
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);

  writer.writeField("address", address);
  writer.writeField("lock_counter", lock_counter);
  writer.writeField("system_inquiry", system_inquiry);
  writer.writeField("system_response", system_response);

  writer.appendKey("bus");
  {
    detail::JsonWriter::Scope busScope(writer,
                                       detail::JsonWriter::Scope::Object);
    writer.writeField("window_us", bus.window_us);
    writer.writeField("offset_us", bus.offset_us);
    writer.writeField("watchdog_timeout_ms", bus.watchdog_timeout_ms);
    writer.writeField("syn_gen", bus.syn_gen);
  }

  writer.appendKey("diagnostics");
  {
    detail::JsonWriter::Scope diagScope(writer,
                                        detail::JsonWriter::Scope::Object);
    writer.writeField("level", toString(diagnostics.level));
    writer.writeField("log_size", diagnostics.log_size);
  }

  writer.appendKey("network");
  {
    detail::JsonWriter::Scope netScope(writer,
                                       detail::JsonWriter::Scope::Object);
    writer.writeField("session_timeout_ms", network.session_timeout_ms);
    writer.writeField("transmit_timeout_ms", network.transmit_timeout_ms);
    writer.writeField("outbound_buffer_size", network.outbound_buffer_size);
  }

  writer.appendKey("device");
  {
    detail::JsonWriter::Scope devScope(writer,
                                       detail::JsonWriter::Scope::Object);
    writer.writeField("scan_on_startup", device.scan_on_startup);
    writer.writeField("initial_delay_s", device.initial_delay_s);
    writer.writeField("startup_interval_s", device.startup_interval_s);
    writer.writeField("max_startup_scans", device.max_startup_scans);
  }

  writer.appendKey("scheduler");
  {
    detail::JsonWriter::Scope schedScope(writer,
                                         detail::JsonWriter::Scope::Object);
    writer.writeField("max_send_attempts", scheduler.max_send_attempts);
    writer.writeField("base_backoff_ms", scheduler.base_backoff_ms);
    writer.writeField("fsm_timeout_ms", scheduler.fsm_timeout_ms);
    writer.writeField("total_timeout_ms", scheduler.total_timeout_ms);
  }
}

RuntimeConfig RuntimeConfig::fromJson(const std::string& json) {
  RuntimeConfig cfg;
  cfg.mergeFromJson(json);
  return cfg;
}

bool RuntimeConfig::mergeFromJson(const std::string& json) {
  detail::JsonReader reader(json);
  if (reader.next() != detail::JsonReader::Token::ObjectStart) return false;

  reader.forEachField([&](std::string_view key, detail::JsonReader& r) {
    if (key == "address") {
      r.next();
      address = static_cast<uint8_t>(r.asNum<int>());
      return true;
    }
    if (key == "lock_counter") {
      r.next();
      lock_counter = static_cast<uint8_t>(r.asNum<int>());
      return true;
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
      if (r.next() == detail::JsonReader::Token::ObjectStart) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "window_us") {
            inner.next();
            bus.window_us = inner.asNum<uint16_t>();
            return true;
          }
          if (k == "offset_us") {
            inner.next();
            bus.offset_us = inner.asNum<uint16_t>();
            return true;
          }
          if (k == "watchdog_timeout_ms") {
            inner.next();
            bus.watchdog_timeout_ms = inner.asNum<uint32_t>();
            return true;
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
      if (r.next() == detail::JsonReader::Token::ObjectStart) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "level") {
            inner.next();
            diagnostics.level = static_cast<LogLevel>(inner.asNum<int>());
            return true;
          }
          if (k == "log_size") {
            inner.next();
            diagnostics.log_size = inner.asNum<size_t>();
            return true;
          }
          return false;
        });
      }
      return true;
    }
    if (key == "network") {
      if (r.next() == detail::JsonReader::Token::ObjectStart) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "session_timeout_ms") {
            inner.next();
            network.session_timeout_ms = inner.asNum<uint32_t>();
            return true;
          }
          if (k == "transmit_timeout_ms") {
            inner.next();
            network.transmit_timeout_ms = inner.asNum<uint32_t>();
            return true;
          }
          if (k == "outbound_buffer_size") {
            inner.next();
            network.outbound_buffer_size = inner.asNum<size_t>();
            return true;
          }
          return false;
        });
      }
      return true;
    }
    if (key == "device") {
      if (r.next() == detail::JsonReader::Token::ObjectStart) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "scan_on_startup") {
            inner.next();
            device.scan_on_startup = inner.asBool();
            return true;
          }
          if (k == "initial_delay_s") {
            inner.next();
            device.initial_delay_s = inner.asNum<uint32_t>();
            return true;
          }
          if (k == "startup_interval_s") {
            inner.next();
            device.startup_interval_s = inner.asNum<uint32_t>();
            return true;
          }
          if (k == "max_startup_scans") {
            inner.next();
            device.max_startup_scans = static_cast<uint8_t>(inner.asNum<int>());
            return true;
          }
          return false;
        });
      }
      return true;
    }
    if (key == "scheduler") {
      if (r.next() == detail::JsonReader::Token::ObjectStart) {
        r.forEachField([&](std::string_view k, detail::JsonReader& inner) {
          if (k == "max_send_attempts") {
            inner.next();
            scheduler.max_send_attempts =
                static_cast<uint8_t>(inner.asNum<int>());
            return true;
          }
          if (k == "base_backoff_ms") {
            inner.next();
            scheduler.base_backoff_ms = inner.asNum<uint32_t>();
            return true;
          }
          if (k == "fsm_timeout_ms") {
            inner.next();
            scheduler.fsm_timeout_ms = inner.asNum<uint32_t>();
            return true;
          }
          if (k == "total_timeout_ms") {
            inner.next();
            scheduler.total_timeout_ms = inner.asNum<uint32_t>();
            return true;
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

bool RuntimeConfig::isValidJson(const std::string& json) {
  return detail::JsonReader::validate(json);
}

void BusConfig::toJson(detail::JsonWriter& writer) const {
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);

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
  detail::JsonWriter::Scope scope(writer, detail::JsonWriter::Scope::Object);
  writer.writeField("runtime", runtime);
  writer.writeField("bus_hardware", bus);
}

}  // namespace ebus