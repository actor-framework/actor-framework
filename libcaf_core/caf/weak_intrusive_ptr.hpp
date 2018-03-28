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
#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include "caf/intrusive_ptr.hpp"
#include "caf/detail/comparable.hpp"

namespace caf {

/// An intrusive, reference counting smart pointer implementation.
/// @relates ref_counted
template <class T>
class weak_intrusive_ptr : detail::comparable<weak_intrusive_ptr<T>>,
                           detail::comparable<weak_intrusive_ptr<T>, const T*>,
                           detail::comparable<weak_intrusive_ptr<T>,
                                              std::nullptr_t> {
public:
  using pointer = T*;
  using const_pointer = const T*;
  using element_type = T;
  using reference = T&;
  using const_reference = const T&;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = true;

  constexpr weak_intrusive_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  weak_intrusive_ptr(pointer raw_ptr, bool add_ref = true) noexcept {
    set_ptr(raw_ptr, add_ref);
  }

  weak_intrusive_ptr(weak_intrusive_ptr&& other) noexcept
      : ptr_(other.detach()) {
    // nop
  }

  weak_intrusive_ptr(const weak_intrusive_ptr& other) noexcept {
    set_ptr(other.get(), true);
  }

  template <class Y>
  weak_intrusive_ptr(weak_intrusive_ptr<Y> other) noexcept
      : ptr_(other.detach()) {
    static_assert(std::is_convertible<Y*, T*>::value,
                  "Y* is not assignable to T*");
  }

  ~weak_intrusive_ptr() {
    if (ptr_)
      intrusive_ptr_release_weak(ptr_);
  }

  void swap(weak_intrusive_ptr& other) noexcept {
    std::swap(ptr_, other.ptr_);
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  pointer detach() noexcept {
    auto result = ptr_;
    if (result)
      ptr_ = nullptr;
    return result;
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  pointer release() noexcept {
    return detach();
  }

  void reset(pointer new_value = nullptr, bool add_ref = true) {
    auto old = ptr_;
    set_ptr(new_value, add_ref);
    if (old)
      intrusive_ptr_release_weak(old);
  }

  weak_intrusive_ptr& operator=(pointer ptr) noexcept {
    reset(ptr);
    return *this;
  }

  weak_intrusive_ptr& operator=(weak_intrusive_ptr other) noexcept {
    swap(other);
    return *this;
  }

  pointer get() const noexcept {
    return ptr_;
  }

  pointer operator->() const noexcept {
    return ptr_;
  }

  reference operator*() const noexcept {
    return *ptr_;
  }

  bool operator!() const noexcept {
    return !ptr_;
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  ptrdiff_t compare(const_pointer ptr) const noexcept {
    return static_cast<ptrdiff_t>(get() - ptr);
  }

  ptrdiff_t compare(const weak_intrusive_ptr& other) const noexcept {
    return compare(other.get());
  }

  ptrdiff_t compare(std::nullptr_t) const noexcept {
    return reinterpret_cast<ptrdiff_t>(get());
  }

  /// Tries to upgrade this weak reference to a strong reference.
  intrusive_ptr<T> lock() const noexcept {
    if (!ptr_ || !intrusive_ptr_upgrade_weak(ptr_))
      return nullptr;
    // reference count already increased by intrusive_ptr_upgrade_weak
    return {ptr_, false};
  }

  /// Tries to upgrade this weak reference to a strong reference.
  /// Returns a pointer with increased strong reference count
  /// on success, `nullptr` otherwise.
  pointer get_locked() const noexcept {
    if (!ptr_ || !intrusive_ptr_upgrade_weak(ptr_))
      return nullptr;
    return ptr_;
  }

private:
  void set_ptr(pointer raw_ptr, bool add_ref) noexcept {
    ptr_ = raw_ptr;
    if (raw_ptr && add_ref)
      intrusive_ptr_add_weak_ref(raw_ptr);
  }

  pointer ptr_;
};

/// @relates weak_intrusive_ptr
template <class X, typename Y>
bool operator==(const weak_intrusive_ptr<X>& lhs,
                const weak_intrusive_ptr<Y>& rhs) {
  return lhs.get() == rhs.get();
}

/// @relates weak_intrusive_ptr
template <class X, typename Y>
bool operator!=(const weak_intrusive_ptr<X>& lhs,
                const weak_intrusive_ptr<Y>& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

