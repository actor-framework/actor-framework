/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include "caf/intrusive_ptr.hpp"

namespace caf {

/// Default implementation for unsharing values.
/// @relates intrusive_cow_ptr
template <class T>
T* default_intrusive_cow_ptr_unshare(T*& ptr) {
  if (!ptr->unique()) {
    auto new_ptr = ptr->copy();
    intrusive_ptr_release(ptr);
    ptr = new_ptr;
  }
  return ptr;
}

/// Customization point for allowing intrusive_cow_ptr<T> with a forward
/// declared T.
/// @relates intrusive_cow_ptr
template <class T>
T* intrusive_cow_ptr_unshare(T*& ptr) {
  return default_intrusive_cow_ptr_unshare(ptr);
}

/// An intrusive, reference counting smart pointer implementation with
/// copy-on-write optimization.
/// @relates ref_counted
/// @relates intrusive_ptr
template <class T>
class intrusive_cow_ptr
    : detail::comparable<intrusive_cow_ptr<T>>,
      detail::comparable<intrusive_cow_ptr<T>, T*>,
      detail::comparable<intrusive_cow_ptr<T>, std::nullptr_t>,
      detail::comparable<intrusive_cow_ptr<T>, intrusive_ptr<T>> {
public:
  // -- member types -----------------------------------------------------------

  using pointer = T*;

  using const_pointer = const T*;

  using element_type = T;

  using reference = T&;

  using const_reference = const T&;

  using counting_pointer = intrusive_ptr<T>;

  // -- constructors, destructors, and assignment operators --------------------

  intrusive_cow_ptr() noexcept = default;

  intrusive_cow_ptr(intrusive_cow_ptr&&) noexcept = default;

  intrusive_cow_ptr(const intrusive_cow_ptr&) noexcept = default;

  template <class Y>
  intrusive_cow_ptr(intrusive_cow_ptr<Y> other) noexcept
      : ptr_(other.detach(), false) {
    // nop
  }

  explicit intrusive_cow_ptr(counting_pointer p) noexcept : ptr_(std::move(p)) {
    // nop
  }

  explicit intrusive_cow_ptr(pointer ptr, bool add_ref = true) noexcept
      : ptr_(ptr, add_ref) {
    // nop
  }

  intrusive_cow_ptr& operator=(intrusive_cow_ptr&&) noexcept = default;

  intrusive_cow_ptr& operator=(const intrusive_cow_ptr&) noexcept = default;

  intrusive_cow_ptr& operator=(counting_pointer x) noexcept {
    ptr_ = std::move(x);
    return *this;
  }

  // -- comparison -------------------------------------------------------------

  ptrdiff_t compare(std::nullptr_t) const noexcept {
    return reinterpret_cast<ptrdiff_t>(get());
  }

  ptrdiff_t compare(const_pointer ptr) const noexcept {
    return reinterpret_cast<intptr_t>(get()) - reinterpret_cast<intptr_t>(ptr);
  }

  ptrdiff_t compare(const counting_pointer& other) const noexcept {
    return compare(other.get());
  }

  ptrdiff_t compare(const intrusive_cow_ptr& other) const noexcept {
    return compare(other.get());
  }

  // -- modifiers --------------------------------------------------------------

  /// Swaps the managed object with `other`.
  void swap(intrusive_cow_ptr& other) noexcept {
    ptr_.swap(other.ptr_);
  }

  /// Replaces the managed object.
  void reset(pointer p = nullptr, bool add_ref = true) noexcept {
    ptr_.reset(p, add_ref);
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  pointer detach() noexcept {
    return ptr_.detach();
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  pointer release() noexcept {
    return ptr_.release();
  }

  /// Forces a copy of the managed object unless it already has a reference
  /// count of exactly 1.
  void unshare() {
    if (ptr_ != nullptr)
      static_cast<void>(unshared());
  }

  /// Returns a mutable pointer to the managed object.
  pointer unshared_ptr() {
    return intrusive_cow_ptr_unshare(ptr_.ptr_);
  }

  /// Returns a mutable reference to the managed object.
  reference unshared() {
    return *unshared_ptr();
  }

  // -- observers --------------------------------------------------------------

  /// Returns a read-only pointer to the managed object.
  const_pointer get() const noexcept {
    return ptr_.get();
  }

  /// Returns the intrusive pointer managing the object.
  const counting_pointer& counting_ptr() const noexcept {
    return ptr_;
  }

  /// Returns a read-only pointer to the managed object.
  const_pointer operator->() const noexcept {
    return get();
  }

  /// Returns a read-only reference to the managed object.
  const_reference operator*() const noexcept {
    return *get();
  }

  explicit operator bool() const noexcept {
    return get() != nullptr;
  }

private:
  // -- member vairables -------------------------------------------------------

  counting_pointer ptr_;
};

/// @relates intrusive_cow_ptr
template <class T>
std::string to_string(const intrusive_cow_ptr<T>& x) {
  return to_string(x.counting_ptr());
}

} // namespace caf
