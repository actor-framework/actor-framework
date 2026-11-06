// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/control_block_ref_count.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::intrusive {

/// Base type for control blocks with atomic strong and weak reference counts.
/// Both counts start at one. The initial weak reference is released when the
/// last strong reference destroys the managed object.
class CAF_CORE_EXPORT atomic_control_block {
public:
  atomic_control_block() noexcept = default;

  atomic_control_block(const atomic_control_block&) = delete;

  atomic_control_block& operator=(const atomic_control_block&) = delete;

  virtual ~atomic_control_block() noexcept;

  /// Destroys the control block and releases its memory.
  virtual void delete_this() noexcept = 0;

  /// Destroys the managed object.
  virtual void destroy_managed() noexcept = 0;

  /// Increments the strong reference count.
  void ref() noexcept {
    ref_count_.inc_strong();
  }

  /// Decrements the strong reference count and destroys the managed object
  /// when the count reaches zero.
  virtual void deref() noexcept;

  /// Increments the weak reference count.
  void ref_weak() noexcept {
    ref_count_.inc_weak();
  }

  /// Decrements the weak reference count and deletes the block at zero.
  void deref_weak() noexcept {
    ref_count_.dec_weak(this);
  }

  /// Tries to add a strong reference unless the strong count has reached zero.
  bool upgrade_weak() noexcept {
    return ref_count_.upgrade_weak();
  }

  /// Returns the current strong reference count.
  size_t strong_reference_count() const noexcept {
    return ref_count_.strong_reference_count();
  }

  /// Returns the current weak reference count, including the implicit one.
  size_t weak_reference_count() const noexcept {
    return ref_count_.weak_reference_count();
  }

protected:
  /// A reference count with values for both strong and weak references.
  detail::control_block_ref_count ref_count_;
};

} // namespace caf::intrusive
