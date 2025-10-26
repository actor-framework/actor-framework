// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <atomic>

namespace caf {

class atomic_ref_count {
public:
  atomic_ref_count() noexcept : count_(1) {
    // nop
  }
  atomic_ref_count(const atomic_ref_count&) = delete;

  atomic_ref_count& operator=(const atomic_ref_count&) = delete;

  void ref() noexcept {
    count_.fetch_add(1, std::memory_order_relaxed);
  }

  template <class Owner>
  void deref(Owner* owner) noexcept {
    if (count_.fetch_sub(1, std::memory_order_release) == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete owner;
    }
  }

private:
  alignas(CAF_CACHE_LINE_SIZE) std::atomic<size_t> count_;
  char padding_[CAF_CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];
};
