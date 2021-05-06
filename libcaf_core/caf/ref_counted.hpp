// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <cstddef>

#include "caf/detail/core_export.hpp"

namespace caf {

/// Base class for reference counted objects with an atomic reference count.
/// Serves the requirements of {@link intrusive_ptr}.
/// @note *All* instances of `ref_counted` start with a reference count of 1.
/// @relates intrusive_ptr
class CAF_CORE_EXPORT ref_counted {
public:
  virtual ~ref_counted();

  ref_counted();
  ref_counted(const ref_counted&);
  ref_counted& operator=(const ref_counted&);

  /// Increases reference count by one.
  void ref() const noexcept {
    rc_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decreases reference count by one and calls `request_deletion`
  /// when it drops to zero.
  void deref() const noexcept;

  /// Queries whether there is exactly one reference.
  bool unique() const noexcept {
    return rc_ == 1;
  }

  size_t get_reference_count() const noexcept {
    return rc_;
  }

  friend void intrusive_ptr_add_ref(const ref_counted* p) noexcept {
    p->ref();
  }

  friend void intrusive_ptr_release(const ref_counted* p) noexcept {
    p->deref();
  }

protected:
  mutable std::atomic<size_t> rc_;
};

} // namespace caf
