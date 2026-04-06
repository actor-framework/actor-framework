// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/add_ref.hpp"
#include "caf/adopt_ref.hpp"
#include "caf/caf_deprecated.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/fwd.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>

namespace caf::detail {

/// Checks whether `T` provides free functions for `intrusive_ptr`.
template <class T>
concept has_intrusive_ptr_free_functions = requires(T*& ptr) {
  { intrusive_ptr_add_ref(ptr) } -> std::same_as<void>;
  { intrusive_ptr_release(ptr) } -> std::same_as<void>;
};

/// Checks whether `T` provides member functions for `intrusive_ptr`.
template <class T>
concept has_intrusive_ptr_member_functions = requires(T* ptr) {
  { ptr->ref() } -> std::same_as<void>;
  { ptr->deref() } -> std::same_as<void>;
};

} // namespace caf::detail

namespace caf {

/// An intrusive, reference counting smart pointer implementation that retains
/// shared ownership of the managed object.
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

  using value_type = T;

  using reference = T&;

  using const_reference = const T&;

  // -- constants --------------------------------------------------------------

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr intrusive_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  constexpr intrusive_ptr(std::nullptr_t) noexcept : intrusive_ptr() {
    // nop
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  intrusive_ptr(pointer ptr, bool increase_ref_count = true) noexcept {
    set_ptr(ptr, increase_ref_count);
  }

  intrusive_ptr(pointer ptr, add_ref_t) noexcept : ptr_(ptr) {
    if (ptr)
      do_ref(ptr_);
  }

  constexpr intrusive_ptr(pointer ptr, adopt_ref_t) noexcept : ptr_(ptr) {
    // nop
  }

  intrusive_ptr(intrusive_ptr&& other) noexcept : ptr_(other.release()) {
    // nop
  }

  intrusive_ptr(const intrusive_ptr& other) noexcept {
    set_ptr(other.ptr_, true);
  }

  template <class Y>
  intrusive_ptr(intrusive_ptr<Y> other) noexcept : ptr_(other.release()) {
    static_assert(std::is_convertible_v<Y*, T*>, "Y* is not assignable to T*");
  }

  ~intrusive_ptr() {
    if (ptr_) {
      do_deref(ptr_);
    }
  }

  void swap(intrusive_ptr& other) noexcept {
    std::swap(ptr_, other.ptr_);
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  [[nodiscard]] pointer release() noexcept {
    if (ptr_ != nullptr) {
      auto result = ptr_;
      ptr_ = nullptr;
      return result;
    }
    return nullptr;
  }

  CAF_DEPRECATED("use release() instead")
  pointer detach() noexcept {
    return release();
  }

  void reset() noexcept {
    if (ptr_) {
      // Must set ptr_ to nullptr BEFORE calling release, because release may
      // trigger destruction of an object that owns this intrusive_ptr. If ptr_
      // is still set when the owner's destructor runs, it would try to release
      // again, causing a double-free.
      auto tmp = ptr_;
      ptr_ = nullptr;
      do_deref(tmp);
    }
  }

  CAF_DEPRECATED("use 'reset(ptr, add_ref)' or 'reset(ptr, adopt_ref)' instead")
  void reset(pointer new_value, bool increase_ref_count = true) noexcept {
    intrusive_ptr tmp{new_value, increase_ref_count};
    swap(tmp);
  }

  void reset(pointer new_value, adopt_ref_t) noexcept {
    intrusive_ptr tmp{new_value, adopt_ref};
    swap(tmp);
  }

  void reset(pointer new_value, add_ref_t) noexcept {
    intrusive_ptr tmp{new_value, add_ref};
    swap(tmp);
  }

  template <class... Ts>
  void emplace(Ts&&... xs) {
    reset(new T(std::forward<Ts>(xs)...), adopt_ref);
  }

  intrusive_ptr& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  CAF_DEPRECATED("use reset instead")
  intrusive_ptr& operator=(pointer ptr) noexcept {
    reset(ptr, add_ref);
    return *this;
  }

  intrusive_ptr& operator=(intrusive_ptr&& other) noexcept {
    swap(other);
    return *this;
  }

  intrusive_ptr& operator=(const intrusive_ptr& other) noexcept {
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
    return ptr_ != nullptr;
  }

  ptrdiff_t compare(const_pointer ptr) const noexcept {
    return static_cast<ptrdiff_t>(ptr_ - ptr);
  }

  ptrdiff_t compare(const intrusive_ptr& other) const noexcept {
    return compare(other.ptr_);
  }

  ptrdiff_t compare(std::nullptr_t) const noexcept {
    return reinterpret_cast<ptrdiff_t>(ptr_);
  }

  size_t hash() const noexcept {
    std::hash<pointer> hasher;
    return hasher(ptr_);
  }

#ifdef CAF_ENABLE_RTTI
  template <class C>
  intrusive_ptr<C> downcast() const noexcept {
    static_assert(std::is_base_of_v<T, C>);
    return {ptr_ ? dynamic_cast<C*>(ptr_) : nullptr, add_ref};
  }
#endif

  template <class C>
  intrusive_ptr<C> upcast() const& noexcept {
    static_assert(std::is_base_of_v<C, T>);
    return {ptr_ ? ptr_ : nullptr, add_ref};
  }

  template <class C>
  intrusive_ptr<C> upcast() && noexcept {
    static_assert(std::is_base_of_v<C, T>);
    return {ptr_ ? release() : nullptr, adopt_ref};
  }

private:
  static void do_ref(pointer ptr) noexcept {
    if constexpr (detail::has_intrusive_ptr_free_functions<T>) {
      intrusive_ptr_add_ref(ptr);
    } else {
      static_assert(detail::has_intrusive_ptr_member_functions<T>);
      ptr->ref();
    }
  }

  static void do_deref(pointer ptr) noexcept {
    if constexpr (detail::has_intrusive_ptr_free_functions<T>) {
      intrusive_ptr_release(ptr);
    } else {
      static_assert(detail::has_intrusive_ptr_member_functions<T>);
      ptr->deref();
    }
  }

  void set_ptr(pointer ptr, bool increase_ref_count) noexcept {
    ptr_ = ptr;
    if (ptr && increase_ref_count) {
      do_ref(ptr);
    }
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
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const intrusive_ptr<T>& lhs, const U* rhs) {
  return lhs.get() == rhs;
}

/// @relates intrusive_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const T* lhs, const intrusive_ptr<U>& rhs) {
  return lhs == rhs.get();
}

/// @relates intrusive_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const intrusive_ptr<T>& lhs, const U* rhs) {
  return lhs.get() != rhs;
}

/// @relates intrusive_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const T* lhs, const intrusive_ptr<U>& rhs) {
  return lhs != rhs.get();
}

// -- comparison to intrusive_pointer ------------------------------------------

/// @relates intrusive_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const intrusive_ptr<T>& x, const intrusive_ptr<U>& y) {
  return x.get() == y.get();
}

/// @relates intrusive_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const intrusive_ptr<T>& x, const intrusive_ptr<U>& y) {
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
  detail::append_hex(result, reinterpret_cast<uintptr_t>(x.get()));
  return result;
}

} // namespace caf

namespace std {

template <class T>
struct hash<caf::intrusive_ptr<T>> {
  size_t operator()(const caf::intrusive_ptr<T>& ptr) const noexcept {
    return ptr.hash();
  }
};

} // namespace std
