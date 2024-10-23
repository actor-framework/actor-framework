// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/core_export.hpp"

#include <atomic>
#include <cstddef>

namespace caf {

/// Base class for reference counted objects with an atomic reference count.
/// Serves the requirements of @ref intrusive_ptr.
/// @note *All* instances of `ref_counted` start with a reference count of 1.
/// @relates intrusive_ptr
class CAF_CORE_EXPORT ref_counted : public detail::atomic_ref_counted {
public:
  using super = detail::atomic_ref_counted;

  using super::super;

  ~ref_counted() override;

  friend void intrusive_ptr_add_ref(const ref_counted* p) noexcept {
    p->ref();
  }

  friend void intrusive_ptr_release(const ref_counted* p) noexcept {
    p->deref();
  }
};

} // namespace caf
