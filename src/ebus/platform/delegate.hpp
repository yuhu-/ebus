/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <type_traits>
#include <utility>

#if defined(ESP_PLATFORM)
#include <esp_attr.h>
#else
#define IRAM_ATTR
#endif

namespace ebus::detail::platform {

/**
 * @brief A high-performance, non-allocating delegate for the Hot Path.
 * Replaces std::function to guarantee zero heap usage and eliminate virtual
 * calls. Supports free functions, member functions, and stateless lambdas.
 */
template <typename T>
class Delegate;

template <typename R, typename... Args>
class Delegate<R(Args...)> {
  using Stub = R (*)(void*, Args...);

 public:
  Delegate() = default;
  Delegate(std::nullptr_t) : instance_(nullptr), stub_(nullptr) {}

  /**
   * @brief Binds a member function of an object instance.
   */
  template <typename T, R (T::*Method)(Args...)>
  static Delegate bind(T* instance) {
    return {
        instance, [](void* obj, Args... args) -> R IRAM_ATTR {
          return (static_cast<T*>(obj)->*Method)(std::forward<Args>(args)...);
        }};
  }

  /**
   * @brief Binds a const member function of an object instance.
   */
  template <typename T, R (T::*Method)(Args...) const>
  static Delegate bind(const T* instance) {
    return {const_cast<T*>(instance),
            [](void* obj, Args... args) -> R IRAM_ATTR {
              return (static_cast<const T*>(obj)->*Method)(
                  std::forward<Args>(args)...);
            }};
  }

  /**
   * @brief Binds a free function or a stateless lambda.
   */
  template <R (*Function)(Args...)>
  static Delegate bind() {
    return {nullptr, [](void*, Args... args) -> R IRAM_ATTR {
              return Function(std::forward<Args>(args)...);
            }};
  }

  /**
   * @brief Implicitly converts stateless lambdas or function pointers.
   */
  template <typename Lambda, typename = std::enable_if_t<
                                 std::is_convertible_v<Lambda, R (*)(Args...)>>>
  Delegate(Lambda l) {
    R (*ptr)(Args...) = l;
    instance_ = reinterpret_cast<void*>(ptr);
    stub_ = [](void* ctx, Args... args) -> R IRAM_ATTR {
      auto p = reinterpret_cast<R (*)(Args...)>(ctx);
      return p(std::forward<Args>(args)...);
    };
  }

  R operator()(Args... args) const {
    // No exception check here for performance; caller should check operator
    // bool()
    return stub_(instance_, std::forward<Args>(args)...);
  }

  explicit operator bool() const noexcept { return stub_ != nullptr; }
  bool operator==(const Delegate& other) const noexcept {
    return instance_ == other.instance_ && stub_ == other.stub_;
  }
  bool operator!=(const Delegate& other) const noexcept {
    return !(*this == other);
  }

 private:
  Delegate(void* instance, Stub stub) : instance_(instance), stub_(stub) {}

  void* instance_ = nullptr;
  Stub stub_ = nullptr;
};

}  // namespace ebus::detail::platform
