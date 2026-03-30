// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/aligned_alloc.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/memory_interface.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

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

  /// Returns the current value of the weak reference counter.
  size_t weak_reference_count() const noexcept {
    return weak_.load(std::memory_order_relaxed);
  }

  /// Increments the strong reference counter by one.
  void inc_strong() noexcept {
    strong_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decrements the strong reference count by one and destroys the managed
  /// object when its strong reference count drops to zero.
  template <class ControlBlock>
  void dec_strong(ControlBlock* control_block) noexcept {
    if (strong_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      if (control_block->delete_managed_object()) {
        dec_weak(control_block);
      }
    }
  }

  /// Decrements the strong reference count by one and returns true if the
  /// reference count dropped to zero. This function is unsafe because it does
  /// not call `delete_managed_object` on the owner if the reference count
  /// dropped to zero. However, it may be used in `delete_managed_object` if
  /// running cleanup code on the managed object may add a new strong reference.
  bool dec_strong_unsafe() noexcept {
    return strong_.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  /// Tries to upgrade a weak reference to a strong reference by incrementing
  /// the strong reference count by one unless it reached 0.
  bool upgrade_weak() noexcept {
    auto count = strong_.load(std::memory_order_relaxed);
    while (count != 0) {
      if (strong_.compare_exchange_weak(count, count + 1,
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
    if (weak_.fetch_add(1, std::memory_order_relaxed) == 0)
      detail::critical("tried to increase the weak reference count "
                       "of an expired object");
#endif
  }

  /// Decrements the weak reference count by one and destroys the control block
  /// when its weak reference count drops to zero.
  template <class ControlBlock>
  void dec_weak(ControlBlock* control_block) noexcept {
    if (weak_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      using control_block_type = std::remove_const_t<ControlBlock>;
      if constexpr (uses_new_and_delete<control_block_type>()) {
        delete control_block;
      } else if constexpr (uses_malloc_and_free<control_block_type>()) {
        static_assert(!std::is_const_v<ControlBlock>,
                      "free() does not accept const pointers");
        control_block->~control_block_type();
        free(control_block);
      } else {
        static_assert(uses_aligned_alloc_and_free<control_block_type>());
        static_assert(!std::is_const_v<ControlBlock>,
                      "aligned_free() does not accept const pointers");
        control_block->~control_block_type();
        aligned_free(control_block);
      }
    }
  }

private:
  std::atomic<size_t> strong_;
  std::atomic<size_t> weak_;
};

} // namespace caf::detail
