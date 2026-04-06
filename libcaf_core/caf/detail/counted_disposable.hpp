// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/atomic_ref_count.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
#include "caf/ref_counted.hpp"

#include <atomic>

namespace caf::detail {

/// Decorates another disposable and creates "nested" disposables. When the last
/// nested disposable is disposed, the parent disposable is also disposed.
class CAF_CORE_EXPORT counted_disposable : public disposable::impl {
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

  void ref() const noexcept final;

  void deref() const noexcept final;

private:
  /// Releases a "reference count" for the disposable.
  void release();

  mutable detail::atomic_ref_count ref_count_;
  disposable decorated_;
  std::atomic<size_t> count_ = 0;
};

} // namespace caf::detail
