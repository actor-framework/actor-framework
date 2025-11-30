// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/concepts.hpp"

#include <cstddef>

namespace caf {

/// A smart pointer for objects created with placement new.
/// This class stores a pointer to an object created via placement new and
/// calls its destructor when the pointer goes out of scope. It does not
/// delete or free the underlying storage.
/// @note This class is not copyable but is movable.
template <class T>
class placement_ptr {
public:
  // -- member types -----------------------------------------------------------

  using pointer = T*;

  using const_pointer = const T*;

  using element_type = T;

  using reference = T&;

  using const_reference = const T&;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr placement_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  constexpr placement_ptr(std::nullptr_t) noexcept : placement_ptr() {
    // nop
  }

  explicit placement_ptr(pointer raw_ptr) noexcept : ptr_(raw_ptr) {
    // nop
  }

  placement_ptr(const placement_ptr&) = delete;

  placement_ptr(placement_ptr&& other) noexcept : ptr_(other.release()) {
    // nop
  }

  ~placement_ptr() {
    if (ptr_)
      ptr_->~T();
  }

  placement_ptr& operator=(const placement_ptr&) = delete;

  placement_ptr& operator=(placement_ptr&& other) noexcept {
    if (this != &other) {
      if (ptr_)
        ptr_->~T();
      ptr_ = other.release();
    }
    return *this;
  }

  placement_ptr& operator=(std::nullptr_t) noexcept {
    if (ptr_) {
      ptr_->~T();
      ptr_ = nullptr;
    }
    return *this;
  }

  // -- modifiers --------------------------------------------------------------

  void reset(pointer new_value = nullptr) noexcept {
    if (ptr_)
      ptr_->~T();
    ptr_ = new_value;
  }

  pointer release() noexcept {
    auto result = ptr_;
    ptr_ = nullptr;
    return result;
  }

  // -- observers --------------------------------------------------------------

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

private:
  pointer ptr_;
};

// -- comparison to nullptr ----------------------------------------------------

/// @relates placement_ptr
template <class T>
bool operator==(const placement_ptr<T>& x, std::nullptr_t) {
  return !x;
}

/// @relates placement_ptr
template <class T>
bool operator==(std::nullptr_t, const placement_ptr<T>& x) {
  return !x;
}

/// @relates placement_ptr
template <class T>
bool operator!=(const placement_ptr<T>& x, std::nullptr_t) {
  return static_cast<bool>(x);
}

/// @relates placement_ptr
template <class T>
bool operator!=(std::nullptr_t, const placement_ptr<T>& x) {
  return static_cast<bool>(x);
}

// -- comparison to raw pointer ------------------------------------------------

/// @relates placement_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const placement_ptr<T>& lhs, const U* rhs) {
  return lhs.get() == rhs;
}

/// @relates placement_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const T* lhs, const placement_ptr<U>& rhs) {
  return lhs == rhs.get();
}

/// @relates placement_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const placement_ptr<T>& lhs, const U* rhs) {
  return lhs.get() != rhs;
}

/// @relates placement_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const T* lhs, const placement_ptr<U>& rhs) {
  return lhs != rhs.get();
}

// -- comparison to placement_ptr ----------------------------------------------

/// @relates placement_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const placement_ptr<T>& x, const placement_ptr<U>& y) {
  return x.get() == y.get();
}

/// @relates placement_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const placement_ptr<T>& x, const placement_ptr<U>& y) {
  return x.get() != y.get();
}

} // namespace caf
