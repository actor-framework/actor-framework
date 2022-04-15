// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

namespace caf::flow {

/// An object that lives on a @ref coordinator.
class CAF_CORE_EXPORT coordinated {
public:
  // -- constructors, destructors, and assignment operators --------------------

  virtual ~coordinated();

  // -- reference counting -----------------------------------------------------

  /// Increases the reference count of the coordinated.
  virtual void ref_coordinated() const noexcept = 0;

  /// Decreases the reference count of the coordinated and destroys the object
  /// if necessary.
  virtual void deref_coordinated() const noexcept = 0;

  friend void intrusive_ptr_add_ref(const coordinated* ptr) noexcept {
    ptr->ref_coordinated();
  }

  friend void intrusive_ptr_release(const coordinated* ptr) noexcept {
    ptr->deref_coordinated();
  }
};

} // namespace caf::flow
