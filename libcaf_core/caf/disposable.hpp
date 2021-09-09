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
class CAF_CORE_EXPORT disposable {
public:
  /// Internal implementation class of a `disposable`.
  class impl {
  public:
    virtual ~impl();

    virtual void dispose() = 0;

    virtual bool disposed() const noexcept = 0;

    disposable as_disposable() noexcept;

    virtual void ref_disposable() const noexcept = 0;

    virtual void deref_disposable() const noexcept = 0;

    friend void intrusive_ptr_add_ref(const impl* ptr) noexcept {
      ptr->ref_disposable();
    }

    friend void intrusive_ptr_release(const impl* ptr) noexcept {
      ptr->deref_disposable();
    }
  };

  explicit disposable(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  disposable() noexcept = default;
  disposable(disposable&&) noexcept = default;
  disposable(const disposable&) noexcept = default;
  disposable& operator=(disposable&&) noexcept = default;
  disposable& operator=(const disposable&) noexcept = default;

  /// Combines multiple disposable into a single disposable. The new disposable
  /// is disposed if all of its elements are disposed. Disposing the composite
  /// disposes all elements individually.
  static disposable make_composite(std::vector<disposable> entries);

  /// Disposes the resource. Calling `dispose()` on a disposed resource is a
  /// no-op.
  void dispose() {
    if (pimpl_) {
      pimpl_->dispose();
      pimpl_ = nullptr;
    }
  }

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

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] intrusive_ptr<impl>&& as_intrusive_ptr() && noexcept {
    return std::move(pimpl_);
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] intrusive_ptr<impl> as_intrusive_ptr() const& noexcept {
    return pimpl_;
  }

  /// Exchanges the content of this handle with `other`.
  void swap(disposable& other) {
    pimpl_.swap(other.pimpl_);
  }

private:
  intrusive_ptr<impl> pimpl_;
};

} // namespace caf
