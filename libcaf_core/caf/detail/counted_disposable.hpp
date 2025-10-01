// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
#include "caf/ref_counted.hpp"

namespace caf::detail {

/// Decorates another disposable and creates "nested" disposables. When the last
/// nested disposable is disposed, the parent disposable is also disposed.
class CAF_CORE_EXPORT counted_disposable : public ref_counted,
                                           public disposable::impl {
public:
  class nested_disposable;

  friend class nested_disposable;

  explicit counted_disposable(disposable decorated)
    : decorated_(std::move(decorated)) {
    // nop
  }

  /// Acquires a new "reference count" for the disposable.
  disposable acquire();

  void dispose() override;

  bool disposed() const noexcept override;

  void ref_disposable() const noexcept override;

  void deref_disposable() const noexcept override;

  friend void intrusive_ptr_add_ref(const counted_disposable* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const counted_disposable* ptr) noexcept {
    ptr->deref();
  }

private:
  /// Releases a "reference count" for the disposable.
  void release();

  disposable decorated_;
  size_t count_ = 0;
};

} // namespace caf::detail
