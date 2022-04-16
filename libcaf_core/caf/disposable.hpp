// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/// Represents a disposable resource.
class CAF_CORE_EXPORT disposable : detail::comparable<disposable> {
public:
  // -- member types -----------------------------------------------------------

  /// Internal implementation class of a `disposable`.
  class CAF_CORE_EXPORT impl {
  public:
    CAF_INTRUSIVE_PTR_FRIENDS_SFX(impl, _disposable)

    virtual ~impl();

    virtual void dispose() = 0;

    virtual bool disposed() const noexcept = 0;

    disposable as_disposable() noexcept;

    virtual void ref_disposable() const noexcept = 0;

    virtual void deref_disposable() const noexcept = 0;
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit disposable(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  disposable() noexcept = default;

  disposable(disposable&&) noexcept = default;

  disposable(const disposable&) noexcept = default;

  disposable& operator=(disposable&&) noexcept = default;

  disposable& operator=(const disposable&) noexcept = default;

  disposable& operator=(std::nullptr_t) noexcept {
    pimpl_ = nullptr;
    return *this;
  }

  // -- factories --------------------------------------------------------------

  /// Combines multiple disposables into a single disposable. The new disposable
  /// is disposed if all of its elements are disposed. Disposing the composite
  /// disposes all elements individually.
  static disposable make_composite(std::vector<disposable> entries);

  // -- mutators ---------------------------------------------------------------

  /// Disposes the resource. Calling `dispose()` on a disposed resource is a
  /// no-op.
  void dispose() {
    if (pimpl_) {
      pimpl_->dispose();
      pimpl_ = nullptr;
    }
  }

  /// Exchanges the content of this handle with `other`.
  void swap(disposable& other) {
    pimpl_.swap(other.pimpl_);
  }

  // -- properties -------------------------------------------------------------

  /// Returns whether the resource has been disposed.
  [[nodiscard]] bool disposed() const noexcept {
    return pimpl_ ? pimpl_->disposed() : true;
  }

  /// Returns whether this handle still points to a resource.
  [[nodiscard]] bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  /// Returns `valid()`;
  explicit operator bool() const noexcept {
    return valid();
  }

  /// Returns `!valid()`;
  bool operator!() const noexcept {
    return !valid();
  }

  /// Returns a pointer to the implementation.
  [[nodiscard]] impl* ptr() const noexcept {
    return pimpl_.get();
  }

  // -- conversions ------------------------------------------------------------

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] intrusive_ptr<impl>&& as_intrusive_ptr() && noexcept {
    return std::move(pimpl_);
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] intrusive_ptr<impl> as_intrusive_ptr() const& noexcept {
    return pimpl_;
  }

  // -- comparisons ------------------------------------------------------------

  /// Compares the internal pointers.
  [[nodiscard]] intptr_t compare(const disposable& other) const noexcept {
    return pimpl_.compare(other.pimpl_);
  }

private:
  intrusive_ptr<impl> pimpl_;
};

/// @relates disposable
using disposable_impl = disposable::impl;

} // namespace caf
