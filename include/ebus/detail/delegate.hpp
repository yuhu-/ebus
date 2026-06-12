/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#if defined(ESP_PLATFORM)
#include <esp_attr.h>
#else
#define IRAM_ATTR
#endif

namespace ebus::detail {

/**
 * @brief A high-performance, non-allocating delegate for the Hot Path.
 * Replaces std::function to guarantee zero heap usage and eliminate virtual
 * calls. Supports free functions, member functions, and stateless lambdas.
 */
template <typename T>
class Delegate;

template <typename R, typename... Args>
class Delegate<R(Args...)> {
  using Invoker = R (*)(const char*, Args...);
  static constexpr std::size_t BufferSize = 24;

  template <typename F>
  static R invoke_stub(const char* buf, Args... args) {
    return (*reinterpret_cast<const F*>(buf))(std::forward<Args>(args)...);
  }

 public:
  Delegate() noexcept = default;
  Delegate(std::nullptr_t) noexcept : invoker_(nullptr) {}

  /**
   * @brief Binds a member function of an object instance.
   */
  template <typename T, R (T::*Method)(Args...)>
  static Delegate bind(T* instance) {
    return Delegate([instance](Args... args) -> R {
      return (instance->*Method)(std::forward<Args>(args)...);
    });
  }

  /**
   * @brief Binds a const member function of an object instance.
   */
  template <typename T, R (T::*Method)(Args...) const>
  static Delegate bind(const T* instance) {
    return Delegate([instance](Args... args) -> R {
      return (instance->*Method)(std::forward<Args>(args)...);
    });
  }

  /**
   * @brief Binds a free function or a stateless lambda.
   */
  template <R (*Function)(Args...)>
  static Delegate bind() {
    return Delegate(Function);
  }

  /**
   * @brief Constructs a delegate from a function pointer or lambda.
   * Uses static_assert to ensure the functor fits in our fixed buffer and is
   * trivially copyable.
   */
  template <typename Functor,
            typename = std::enable_if_t<
                !std::is_same_v<std::decay_t<Functor>, Delegate> &&
                std::is_invocable_r_v<R, Functor, Args...>>>
  // cppcheck-suppress noExplicitConstructor
  Delegate(Functor&& f) {
    using F = std::decay_t<Functor>;
    static_assert(sizeof(F) <= BufferSize,
                  "Functor too large for Delegate buffer");
    static_assert(std::is_trivially_copyable_v<F>,
                  "Functor must be trivially copyable to be stored in the "
                  "Delegate buffer");

    std::memcpy(buffer_, &f, sizeof(F));
    invoker_ = &invoke_stub<F>;
  }

  R operator()(Args... args) const {
    return invoker_(buffer_, std::forward<Args>(args)...);
  }

  explicit operator bool() const noexcept { return invoker_ != nullptr; }

  bool operator==(const Delegate& other) const noexcept {
    if (invoker_ != other.invoker_) return false;
    if (invoker_ == nullptr) return true;
    return std::memcmp(buffer_, other.buffer_, BufferSize) == 0;
  }

  bool operator!=(const Delegate& other) const noexcept {
    return !(*this == other);
  }

 private:
  alignas(std::max_align_t) char buffer_[BufferSize]{};
  Invoker invoker_ = nullptr;
};

}  // namespace ebus::detail
