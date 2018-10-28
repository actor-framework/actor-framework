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

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "caf/detail/append_hex.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// An intrusive, reference counting smart pointer implementation.
/// @relates ref_counted
template <class T>
class intrusive_ptr {
public:
  // -- friends ----------------------------------------------------------------

  template <class>
  friend class intrusive_cow_ptr;

  // -- member types -----------------------------------------------------------

  using pointer = T*;

  using const_pointer = const T*;

  using element_type = T;

  using reference = T&;

  using const_reference = const T&;

  // -- constants --------------------------------------------------------------

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr intrusive_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  intrusive_ptr(pointer raw_ptr, bool add_ref = true) noexcept {
    set_ptr(raw_ptr, add_ref);
  }

  intrusive_ptr(intrusive_ptr&& other) noexcept : ptr_(other.detach()) {
    // nop
  }

  intrusive_ptr(const intrusive_ptr& other) noexcept {
    set_ptr(other.get(), true);
  }

  template <class Y>
  intrusive_ptr(intrusive_ptr<Y> other) noexcept : ptr_(other.detach()) {
    static_assert(std::is_convertible<Y*, T*>::value,
                  "Y* is not assignable to T*");
  }

  ~intrusive_ptr() {
    if (ptr_)
      intrusive_ptr_release(ptr_);
  }

  void swap(intrusive_ptr& other) noexcept {
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

  void reset(pointer new_value = nullptr, bool add_ref = true) noexcept {
    auto old = ptr_;
    set_ptr(new_value, add_ref);
    if (old)
      intrusive_ptr_release(old);
  }

  intrusive_ptr& operator=(pointer ptr) noexcept {
    reset(ptr);
    return *this;
  }

  intrusive_ptr& operator=(intrusive_ptr other) noexcept {
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
    return ptr_ != nullptr;
  }

  ptrdiff_t compare(const_pointer ptr) const noexcept {
    return static_cast<ptrdiff_t>(get() - ptr);
  }

  ptrdiff_t compare(const intrusive_ptr& other) const noexcept {
    return compare(other.get());
  }

  ptrdiff_t compare(std::nullptr_t) const noexcept {
    return reinterpret_cast<ptrdiff_t>(get());
  }

  template <class C>
  intrusive_ptr<C> downcast() const noexcept {
    return (ptr_) ? dynamic_cast<C*>(get()) : nullptr;
  }

  template <class C>
  intrusive_ptr<C> upcast() const noexcept {
    return (ptr_) ? static_cast<C*>(get()) : nullptr;
  }

private:
  void set_ptr(pointer raw_ptr, bool add_ref) noexcept {
    ptr_ = raw_ptr;
    if (raw_ptr && add_ref)
      intrusive_ptr_add_ref(raw_ptr);
  }

  pointer ptr_;
};

// -- comparison to nullptr ----------------------------------------------------

/// @relates intrusive_ptr
template <class T>
bool operator==(const intrusive_ptr<T>& x, std::nullptr_t) {
  return !x;
}

/// @relates intrusive_ptr
template <class T>
bool operator==(std::nullptr_t, const intrusive_ptr<T>& x) {
  return !x;
}

/// @relates intrusive_ptr
template <class T>
bool operator!=(const intrusive_ptr<T>& x, std::nullptr_t) {
  return static_cast<bool>(x);
}

/// @relates intrusive_ptr
template <class T>
bool operator!=(std::nullptr_t, const intrusive_ptr<T>& x) {
  return static_cast<bool>(x);
}

// -- comparison to raw pointer ------------------------------------------------

/// @relates intrusive_ptr
template <class T>
bool operator==(const intrusive_ptr<T>& x, const T* y) {
  return x.get() == y;
}

/// @relates intrusive_ptr
template <class T>
bool operator==(const T* x, const intrusive_ptr<T>& y) {
  return x == y.get();
}

/// @relates intrusive_ptr
template <class T>
bool operator!=(const intrusive_ptr<T>& x, const T* y) {
  return x.get() != y;
}

/// @relates intrusive_ptr
template <class T>
bool operator!=(const T* x, const intrusive_ptr<T>& y) {
  return x != y.get();
}

// -- comparison to intrusive_pointer ------------------------------------------

/// @relates intrusive_ptr
template <class T, class U>
detail::enable_if_t<detail::is_comparable<T*, U*>::value, bool>
operator==(const intrusive_ptr<T>& x, const intrusive_ptr<U>& y) {
  return x.get() == y.get();
}

/// @relates intrusive_ptr
template <class T, class U>
detail::enable_if_t<detail::is_comparable<T*, U*>::value, bool>
operator!=(const intrusive_ptr<T>& x, const intrusive_ptr<U>& y) {
  return x.get() != y.get();
}

/// @relates intrusive_ptr
template <class T>
bool operator<(const intrusive_ptr<T>& x, const intrusive_ptr<T>& y) {
  return x.get() < y.get();
}

/// @relates intrusive_ptr
template <class T>
bool operator<(const intrusive_ptr<T>& x, const T* y) {
  return x.get() < y;
}
/// @relates intrusive_ptr
template <class T>
bool operator<(const T* x, const intrusive_ptr<T>& y) {
  return x < y.get();
}

template <class T>
std::string to_string(const intrusive_ptr<T>& x) {
  std::string result;
  auto v = reinterpret_cast<uintptr_t>(x.get());
  detail::append_hex(result, reinterpret_cast<uint8_t*>(&v), sizeof(v));
  return result;
}

} // namespace caf

