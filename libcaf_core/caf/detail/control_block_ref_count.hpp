// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/critical.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>

namespace caf::detail {

/// An atomic reference count for a control block. Supports both strong and weak
/// references.
class control_block_ref_count {
public:
  constexpr control_block_ref_count() noexcept : strong_(1), weak_(1) {
    // nop
  }

  constexpr control_block_ref_count(control_block_ref_count&&) noexcept
    : strong_(1), weak_(1) {
    // Intentionally don't copy the reference count.
  }

  constexpr control_block_ref_count(const control_block_ref_count&) noexcept
    : strong_(1), weak_(1) {
    // Intentionally don't copy the reference count.
  }

  constexpr control_block_ref_count&
  operator=(control_block_ref_count&&) noexcept {
    // Intentionally don't overwrite the reference counts.
    return *this;
  }

  constexpr control_block_ref_count&
  operator=(const control_block_ref_count&) noexcept {
    // Intentionally don't overwrite the reference counts.
    return *this;
  }

  /// Returns the current value of the strong reference counter.
  size_t strong_reference_count() const noexcept {
    return strong_.load(std::memory_order_relaxed);
  }

  /// Returns a reference to the strong reference counter.
  auto& strong_reference_count_ref() noexcept {
    return strong_;
  }

  /// Returns the current value of the weak reference counter.
  size_t weak_reference_count() const noexcept {
    return weak_.load(std::memory_order_relaxed);
  }

  /// Returns a reference to the weak reference counter.
  auto& weak_reference_count_ref() noexcept {
    return weak_;
  }

  /// Increments the strong reference counter by one.
  void inc_strong() noexcept {
#ifdef NDEBUG
    strong_.fetch_add(1, std::memory_order_relaxed);
#else
    if (strong_.fetch_add(1, std::memory_order_relaxed) == 0) {
      detail::critical("tried to increase the strong reference count "
                       "of an expired object");
    }
#endif
  }

  /// Decrements the strong reference count by one and destroys the managed
  /// object when its strong reference count drops to zero.
  template <class ControlBlock>
  void dec_strong(ControlBlock* control_block) noexcept {
    if (strong_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      control_block->destroy_managed();
      dec_weak(control_block);
    }
  }

  /// Compare-and-swap on the strong reference count.
  bool cas_strong(size_t& current_value, size_t new_value,
                  std::memory_order success_order,
                  std::memory_order failure_order) noexcept {
    return strong_.compare_exchange_weak(current_value, new_value,
                                         success_order, failure_order);
  }

  /// Tries to upgrade a weak reference to a strong reference by incrementing
  /// the strong reference count by one unless it reached 0.
  bool upgrade_weak() noexcept {
    auto count = strong_.load(std::memory_order_relaxed);
    while (count != 0) {
      if (strong_.compare_exchange_weak(count, count + 1,
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed))
        return true;
    }
    return false;
  }

  /// Increments the weak reference counter by one.
  void inc_weak() noexcept {
#ifdef NDEBUG
    weak_.fetch_add(1, std::memory_order_relaxed);
#else
    if (weak_.fetch_add(1, std::memory_order_relaxed) == 0) {
      detail::critical("tried to increase the weak reference count "
                       "of an expired object");
    }
#endif
  }

  /// Decrements the weak reference count by one and destroys the control block
  /// when its weak reference count drops to zero.
  template <class ControlBlock>
  void dec_weak(ControlBlock* control_block) noexcept {
    if (weak_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      control_block->delete_this();
    }
  }

private:
  std::atomic<size_t> strong_;
  std::atomic<size_t> weak_;
};

} // namespace caf::detail
