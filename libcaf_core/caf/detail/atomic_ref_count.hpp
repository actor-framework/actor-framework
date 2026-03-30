// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/critical.hpp"
#include "caf/detail/memory_interface.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

namespace caf::detail {

/// An atomic reference count for reference counted objects. Only supports
/// strong references. The reference count is initialized to 1 at construction.
class atomic_ref_count {
public:
  constexpr atomic_ref_count() noexcept : value_(1) {
    // nop
  }

  constexpr atomic_ref_count(atomic_ref_count&&) noexcept : value_(1) {
    // Intentionally don't copy the reference count.
  }

  constexpr atomic_ref_count(const atomic_ref_count&) noexcept : value_(1) {
    // Intentionally don't copy the reference count.
  }

  constexpr atomic_ref_count& operator=(atomic_ref_count&&) noexcept {
    // Intentionally don't overwrite the reference count.
    return *this;
  }

  constexpr atomic_ref_count& operator=(const atomic_ref_count&) noexcept {
    // Intentionally don't overwrite the reference count.
    return *this;
  }

  /// Returns the current value of the reference count.
  size_t value() const noexcept {
    return value_.load(std::memory_order_relaxed);
  }

  /// Increments the reference count by one.
  void inc() noexcept {
#ifdef NDEBUG
    value_.fetch_add(1, std::memory_order_relaxed);
#else
    if (value_.fetch_add(1, std::memory_order_relaxed) == 0) {
      detail::critical("tried to increase the reference count "
                       "of an expired object");
    }
#endif
  }

  /// Decrements the reference count by one and destroys the object when its
  /// reference count drops to zero.
  template <class Owner>
  void dec(Owner* owner) noexcept {
    if (value_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      using owner_type = std::remove_const_t<Owner>;
      if constexpr (uses_new_and_delete<owner_type>) {
        delete owner;
      } else if constexpr (uses_malloc_and_free<owner_type>) {
        static_assert(!std::is_const_v<Owner>,
                      "free() does not accept const pointers");
        owner->~owner_type();
        free(owner);
      } else {
        static_assert(uses_aligned_alloc_and_free<owner_type>);
        static_assert(!std::is_const_v<Owner>,
                      "aligned_free() does not accept const pointers");
        owner->~owner_type();
        aligned_free(owner);
      }
    }
  }

private:
  std::atomic<size_t> value_;
};

} // namespace caf::detail
