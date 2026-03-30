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
#include "caf/intrusive_ptr.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>

namespace caf::detail {

/// Checks whether `T` can be used with `strong_ptr` and `weak_ptr`.
template <class T>
concept managed_object = requires(T) {
  { has_intrusive_ptr_member_functions<typename T::control_block_type> };
};

} // namespace caf::detail

namespace caf {

/// Similar to `intrusive_ptr`, but requires that `T` is managed using a control
/// block. Also grants access to the control block. Unlike the design of
/// `std::shared_ptr`, the control block cannot be allocated separately.
///
/// Note that the same object can be managed by `strong_ptr` and `intrusive_ptr`
/// if `T` implements `ref()` and `deref()`. However, `weak_ptr` is only
/// constructible from a `strong_ptr`.
template <class T>
class strong_ptr {
public:
  // -- member types -----------------------------------------------------------

  static_assert(detail::managed_object<T>);

  using control_block_type = typename T::control_block_type;

  using control_block_pointer = control_block_type*;

  using pointer = T*;

  using const_pointer = const T*;

  using element_type = T;

  using value_type = T;

  using reference = T&;

  using const_reference = const T&;

  using managed_type = typename control_block_type::managed_type;

  static constexpr bool is_base_ptr_type = std::is_same_v<T, managed_type>;

  // -- constants --------------------------------------------------------------

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr strong_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  constexpr strong_ptr(std::nullptr_t) noexcept : strong_ptr() {
    // nop
  }

  strong_ptr(control_block_pointer ptr, add_ref_t) noexcept
    requires is_base_ptr_type
    : ptr_(ptr) {
    if (ptr) {
      ptr->ref();
    }
  }

  constexpr strong_ptr(control_block_pointer ptr, adopt_ref_t) noexcept
    requires is_base_ptr_type
    : ptr_(ptr) {
    // nop
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  strong_ptr(pointer ptr, bool increase_ref_count = true) noexcept {
    set_ptr(ctrl(ptr), increase_ref_count);
  }

  strong_ptr(pointer ptr, add_ref_t) noexcept : ptr_(ctrl(ptr)) {
    if (ptr)
      ptr_->ref();
  }

  constexpr strong_ptr(pointer ptr, adopt_ref_t) noexcept : ptr_(ctrl(ptr)) {
    // nop
  }

  strong_ptr(strong_ptr&& other) noexcept : ptr_(other.ptr_) {
    if (ptr_) {
      other.ptr_ = nullptr;
    }
  }

  strong_ptr(const strong_ptr& other) noexcept : ptr_(other.ptr_) {
    if (ptr_) {
      ptr_->ref();
    }
  }

  template <class Y>
    requires(std::is_convertible_v<Y*, T*>)
  strong_ptr(strong_ptr<Y> other) noexcept
    : strong_ptr(other.release(), adopt_ref) {
    // nop
  }

  ~strong_ptr() {
    if (ptr_) {
      ptr_->deref();
    }
  }

  void swap(strong_ptr& other) noexcept {
    std::swap(ptr_, other.ptr_);
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  [[nodiscard]] pointer release() noexcept {
    if (ptr_ != nullptr) {
      auto result = get();
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
      // trigger destruction of an object that owns this strong_ptr. If ptr_
      // is still set when the owner's destructor runs, it would try to release
      // again, causing a double-free.
      auto tmp = ptr_;
      ptr_ = nullptr;
      tmp->deref();
    }
  }

  CAF_DEPRECATED("use 'reset(ptr, add_ref)' or 'reset(ptr, adopt_ref)' instead")
  void reset(pointer ptr, bool increase_ref_count = true) noexcept {
    strong_ptr tmp{ptr, increase_ref_count};
    swap(tmp);
  }

  void reset(pointer ptr, adopt_ref_t) noexcept {
    strong_ptr tmp{ptr, adopt_ref};
    swap(tmp);
  }

  void reset(pointer ptr, add_ref_t) noexcept {
    strong_ptr tmp{ptr, add_ref};
    swap(tmp);
  }

  void reset(control_block_pointer ptr, adopt_ref_t) noexcept
    requires is_base_ptr_type
  {
    strong_ptr tmp{ptr, adopt_ref};
    swap(tmp);
  }

  void reset(control_block_pointer ptr, add_ref_t) noexcept
    requires is_base_ptr_type
  {
    strong_ptr tmp{ptr, add_ref};
    swap(tmp);
  }

  /// Constructs an object of type `U` in an `strong_ptr`. This factory
  /// function is similar to `make_counted`, but it allows passing a derived
  /// type of `T` as the template parameter. This allows constructing an object
  /// on a derived type while using the base type for the pointer.
  template <class U = T, class... Ts>
  static strong_ptr make(Ts&&... xs) {
    static_assert(std::is_convertible_v<U*, T*>);
    return {new U(std::forward<Ts>(xs)...), adopt_ref};
  }

  strong_ptr& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  CAF_DEPRECATED("use reset instead")
  strong_ptr& operator=(pointer ptr) noexcept {
    reset(ptr, add_ref);
    return *this;
  }

  strong_ptr& operator=(strong_ptr&& other) noexcept {
    swap(other);
    return *this;
  }

  strong_ptr& operator=(const strong_ptr& other) noexcept {
    reset(other.ptr_, add_ref);
    return *this;
  }

  control_block_pointer control_block() const noexcept {
    return ptr_;
  }

  pointer get() const noexcept {
    if (ptr_) {
      if constexpr (is_base_ptr_type) {
        return control_block_type::managed(ptr_);
      } else {
        // This cast is safe, because the constructor will only accept a
        // `pointer` parameter. Hence, we know that the pointer must point to
        // the derived type.
        return static_cast<pointer>(control_block_type::managed(ptr_));
      }
    }
    return nullptr;
  }

  pointer operator->() const noexcept {
    return get();
  }

  reference operator*() const noexcept {
    return *get();
  }

  bool operator!() const noexcept {
    return !ptr_;
  }

  explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

  ptrdiff_t compare(control_block_pointer ptr) const noexcept {
    return static_cast<ptrdiff_t>(ptr_ - ptr);
  }

  ptrdiff_t compare(const strong_ptr& other) const noexcept {
    return compare(other.ptr_);
  }

  ptrdiff_t compare(std::nullptr_t) const noexcept {
    return reinterpret_cast<ptrdiff_t>(get());
  }

#ifdef CAF_ENABLE_RTTI
  template <class C>
  strong_ptr<C> downcast() const noexcept {
    static_assert(std::is_base_of_v<T, C>);
    return {ptr_ ? dynamic_cast<C*>(get()) : nullptr, add_ref};
  }
#endif

  template <class C>
  strong_ptr<C> upcast() const& noexcept {
    static_assert(std::is_base_of_v<C, T>);
    return {ptr_ ? get() : nullptr, add_ref};
  }

  template <class C>
  strong_ptr<C> upcast() && noexcept {
    static_assert(std::is_base_of_v<C, T>);
    return {ptr_ ? release() : nullptr, adopt_ref};
  }

private:
  static control_block_pointer ctrl(managed_type* ptr) noexcept {
    if (ptr) {
      // The memory layout is enforced by `make_actor`.
      return reinterpret_cast<control_block_pointer>(
        reinterpret_cast<intptr_t>(ptr) - control_block_type::allocation_size);
    }
    return nullptr;
  }

  void set_ptr(control_block_pointer ptr, bool increase_ref_count) noexcept {
    ptr_ = ptr;
    if (ptr && increase_ref_count) {
      ptr->ref();
    }
  }

  control_block_pointer ptr_;
};

// -- comparison to nullptr ----------------------------------------------------

/// @relates strong_ptr
template <class T>
bool operator==(const strong_ptr<T>& x, std::nullptr_t) {
  return !x;
}

/// @relates strong_ptr
template <class T>
bool operator==(std::nullptr_t, const strong_ptr<T>& x) {
  return !x;
}

/// @relates strong_ptr
template <class T>
bool operator!=(const strong_ptr<T>& x, std::nullptr_t) {
  return static_cast<bool>(x);
}

/// @relates strong_ptr
template <class T>
bool operator!=(std::nullptr_t, const strong_ptr<T>& x) {
  return static_cast<bool>(x);
}

// -- comparison to raw pointer ------------------------------------------------

/// @relates strong_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const strong_ptr<T>& lhs, const U* rhs) {
  return lhs.get() == rhs;
}

/// @relates strong_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const T* lhs, const strong_ptr<U>& rhs) {
  return lhs == rhs.get();
}

/// @relates strong_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const strong_ptr<T>& lhs, const U* rhs) {
  return lhs.get() != rhs;
}

/// @relates strong_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const T* lhs, const strong_ptr<U>& rhs) {
  return lhs != rhs.get();
}

// -- comparison to intrusive_pointer ------------------------------------------

/// @relates strong_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator==(const strong_ptr<T>& x, const strong_ptr<U>& y) {
  return x.get() == y.get();
}

/// @relates strong_ptr
template <class T, class U>
  requires detail::is_comparable<T*, U*>
bool operator!=(const strong_ptr<T>& x, const strong_ptr<U>& y) {
  return x.get() != y.get();
}

/// @relates strong_ptr
template <class T>
bool operator<(const strong_ptr<T>& x, const strong_ptr<T>& y) {
  return x.get() < y.get();
}

/// @relates strong_ptr
template <class T>
bool operator<(const strong_ptr<T>& x, const T* y) {
  return x.get() < y;
}
/// @relates strong_ptr
template <class T>
bool operator<(const T* x, const strong_ptr<T>& y) {
  return x < y.get();
}

template <class T>
std::string to_string(const strong_ptr<T>& x) {
  std::string result;
  detail::append_hex(result, reinterpret_cast<uintptr_t>(x.get()));
  return result;
}

} // namespace caf
