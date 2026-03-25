// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_count.hpp"
#include "caf/detail/core_export.hpp"

#include <cstddef>

namespace caf {

/// Base class for reference counted objects with an atomic reference count.
/// Serves the requirements of @ref intrusive_ptr.
/// @note *All* instances of `ref_counted` start with a reference count of 1.
/// @relates intrusive_ptr
class CAF_CORE_EXPORT ref_counted {
public:
  virtual ~ref_counted() noexcept;

  void ref() const noexcept {
    ref_count_.inc();
  }

  void deref() const noexcept {
    ref_count_.dec(this);
  }

  bool unique() const noexcept {
    return ref_count_.unique();
  }

  size_t get_reference_count() const noexcept {
    return ref_count_.value();
  }

  friend void intrusive_ptr_add_ref(const ref_counted* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const ref_counted* ptr) noexcept {
    ptr->deref();
  }

private:
  mutable detail::atomic_ref_count ref_count_;
};

} // namespace caf
