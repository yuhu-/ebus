/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>
#include <ebus/utils.hpp>

#include "core/bus_monitor.hpp"

using namespace ebus::detail;

TEST_CASE("BusMonitor: Derived Metrics Logic", "[core][telemetry]") {
  BusMonitor monitor;

  SECTION("Quality Score calculation") {
    // Mock 10% error rate and 20% contention
    monitor.updateHandler([](auto& m) {
      m.messages_passive_broadcast = 90;
      m.error_passive_master = 10;
    });
    monitor.updateRequest([](auto& m) {
      m.first_won = 80;
      m.first_lost = 20;
    });

    auto metrics = monitor.getMetrics();
    // Error Rate = 10 / (90+10) = 10%
    // Contention Rate = 20 / (80+20) = 20%
    // Quality = (100 - 10) * (1.0 - 0.2) = 90 * 0.8 = 72%
    REQUIRE(metrics.handler.error_rate == Catch::Approx(10.0));
    REQUIRE(metrics.request.contention_rate == Catch::Approx(20.0));
    REQUIRE(metrics.quality == Catch::Approx(72.0));
  }

  SECTION("Congestion detection threshold") {
    // Simulate 80% utilization for 11 seconds
    monitor.uptime.addSample(11000000.0);  // 11s in us
    // 80% of 11s = 8.8s
    monitor.utilization.addSample(8800000.0);

    auto metrics = monitor.getMetrics();
    REQUIRE(metrics.bus.utilization == Catch::Approx(80.0));
    REQUIRE(metrics.bus.congestion == true);
  }
}
