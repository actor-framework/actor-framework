// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/intrusive_ptr.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace caf {

/// An intrusive, reference counting smart pointer implementation.
/// @relates ref_counted
template <class T>
class weak_intrusive_ptr
  : detail::comparable<weak_intrusive_ptr<T>>,
    detail::comparable<weak_intrusive_ptr<T>, const T*>,
    detail::comparable<weak_intrusive_ptr<T>, std::nullptr_t> {
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

  [[deprecated("construct using add_ref or adopt_ref instead")]]
  weak_intrusive_ptr(pointer raw_ptr, bool increase_ref_count = true) noexcept {
    set_ptr(raw_ptr, increase_ref_count);
  }

  weak_intrusive_ptr(pointer raw_ptr, add_ref_t) noexcept {
    if (raw_ptr) {
      ptr_ = raw_ptr;
      intrusive_ptr_add_weak_ref(ptr_);
    } else {
      ptr_ = nullptr;
    }
  }

  constexpr weak_intrusive_ptr(pointer raw_ptr, adopt_ref_t) noexcept
    : ptr_(raw_ptr) {
    // nop
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
    static_assert(std::is_convertible_v<Y*, T*>, "Y* is not assignable to T*");
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

  void reset() noexcept {
    if (ptr_) {
      // Must set ptr_ to nullptr BEFORE calling release, because release may
      // trigger destruction of an object that owns this weak_intrusive_ptr. If
      // ptr_ is still set when the owner's destructor runs, it would try to
      // release again, causing a double-free.
      auto tmp = ptr_;
      ptr_ = nullptr;
      intrusive_ptr_release_weak(tmp);
    }
  }

  [[deprecated("use 'reset(ptr, add_ref)' or 'reset(ptr, adopt_ref)' instead")]]
  void reset(pointer new_value, bool increase_ref_count = true) {
    auto old = ptr_;
    set_ptr(new_value, increase_ref_count);
    if (old)
      intrusive_ptr_release_weak(old);
  }

  void reset(pointer new_value, add_ref_t) noexcept {
    weak_intrusive_ptr tmp{new_value, add_ref};
    swap(tmp);
  }

  void reset(pointer new_value, adopt_ref_t) noexcept {
    weak_intrusive_ptr tmp{new_value, adopt_ref};
    swap(tmp);
  }

  weak_intrusive_ptr& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  [[deprecated("use reset instead")]]
  weak_intrusive_ptr& operator=(pointer ptr) noexcept {
    reset(ptr, add_ref);
    return *this;
  }

  weak_intrusive_ptr& operator=(weak_intrusive_ptr&& other) noexcept {
    swap(other);
    return *this;
  }

  weak_intrusive_ptr& operator=(const weak_intrusive_ptr& other) noexcept {
    reset(other.ptr_, add_ref);
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
    return {ptr_, adopt_ref};
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
  void set_ptr(pointer raw_ptr, bool increase_ref_count) noexcept {
    ptr_ = raw_ptr;
    if (raw_ptr && increase_ref_count)
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
