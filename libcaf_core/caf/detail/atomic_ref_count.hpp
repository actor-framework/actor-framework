// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/memory_interface.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace caf::detail {

/// An atomic reference count for reference counted objects.
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

  bool unique() const noexcept {
    return value_.load() == 1;
  }

  size_t value() const noexcept {
    return value_.load();
  }

  void inc() noexcept {
    value_.fetch_add(1, std::memory_order_relaxed);
  }

  template <class Owner>
  void dec(Owner* owner) noexcept {
    if (value_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      using owner_type = std::remove_const_t<Owner>;
      if constexpr (has_memory_interface<owner_type>) {
        if constexpr (owner_type::memory_interface
                      == memory_interface::new_and_delete) {
          delete owner;
        } else {
          static_assert(owner_type::memory_interface
                        == memory_interface::malloc_and_free);
          owner->~owner_type();
          free(owner);
        }
      } else {
        delete owner;
      }
    }
  }

private:
  std::atomic<size_t> value_;
};

} // namespace caf::detail
