/*
 * Copyright (C) 2026 Roland Jax
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "platform/mutex.hpp"

namespace ebus::detail {

class BusAccessPermit {
 public:
  enum Owner { idle, scheduler, external };

  // Only ONE of these can hold the permit at a time
  bool tryAcquire(Owner owner) {
    platform::LockGuard<platform::Mutex> lock(arbiter_lock_);
    if (current_owner_ != idle && current_owner_ != owner)
      return false;  // Someone else has it
    current_owner_ = owner;
    return true;
  }

  void release(Owner owner) {
    platform::LockGuard<platform::Mutex> lock(arbiter_lock_);
    if (current_owner_ == owner) current_owner_ = idle;
  }

 private:
  Owner current_owner_{idle};
  platform::Mutex arbiter_lock_;
};

}  // namespace ebus::detail