/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_all.hpp>

#include "platform/delegate.hpp"

using namespace ebus::detail::platform;

namespace {
struct MockObject {
  int value = 0;
  void update(int v) { value = v; }
  void increment() { value++; }
};

int g_value = 0;
void freeFunction(int v) { g_value = v; }
}  // namespace

TEST_CASE("Delegate: Basic Binding", "[platform][delegate]") {
  SECTION("Member function with arguments") {
    MockObject obj;
    auto d = Delegate<void(int)>::bind<MockObject, &MockObject::update>(&obj);
    REQUIRE(d);
    d(42);
    REQUIRE(obj.value == 42);
  }

  SECTION("Member function without arguments") {
    MockObject obj;
    auto d = Delegate<void()>::bind<MockObject, &MockObject::increment>(&obj);
    d();
    REQUIRE(obj.value == 1);
  }

  SECTION("Free function binding") {
    auto d = Delegate<void(int)>::bind<&freeFunction>();
    g_value = 0;
    d(100);
    REQUIRE(g_value == 100);
  }

  SECTION("Stateless lambda implicit conversion") {
    g_value = 0;
    Delegate<void(int)> d = [](int v) { g_value = v; };
    REQUIRE(d);
    d(50);
    REQUIRE(g_value == 50);
  }
}
