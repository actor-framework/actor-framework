// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <cstddef>

#include "caf/detail/core_export.hpp"
#include "caf/detail/ref_counted_base.hpp"

namespace caf {

/// Base class for reference counted objects with an atomic reference count.
/// Serves the requirements of {@link intrusive_ptr}.
/// @note *All* instances of `ref_counted` start with a reference count of 1.
/// @relates intrusive_ptr
class CAF_CORE_EXPORT ref_counted : public detail::ref_counted_base {
public:
  using super = ref_counted_base;

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
