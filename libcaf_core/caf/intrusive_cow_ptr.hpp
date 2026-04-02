// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/add_ref.hpp"
#include "caf/adopt_ref.hpp"
#include "caf/caf_deprecated.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>
#include <cstdint>
#include <tuple>

namespace caf::detail {

template <class T>
concept has_intrusive_cow_ptr_unshare = requires(T*& ptr) {
  { intrusive_cow_ptr_unshare(ptr) } -> std::same_as<T*>;
};

} // namespace caf::detail

namespace caf {

/// An intrusive, reference counting smart pointer implementation with
/// copy-on-write optimization.
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

  intrusive_cow_ptr(std::nullptr_t) noexcept {
    // nop
  }

  template <class Y>
  intrusive_cow_ptr(intrusive_cow_ptr<Y> other) noexcept
    : ptr_(other.release(), adopt_ref) {
    // nop
  }

  explicit intrusive_cow_ptr(counting_pointer p) noexcept : ptr_(std::move(p)) {
    // nop
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  explicit intrusive_cow_ptr(pointer ptr,
                             bool increase_ref_count = true) noexcept
    : ptr_(ptr, increase_ref_count) {
    // nop
  }

  intrusive_cow_ptr(pointer ptr, add_ref_t) noexcept : ptr_(ptr, add_ref) {
    // nop
  }

  intrusive_cow_ptr(pointer ptr, adopt_ref_t) noexcept : ptr_(ptr, adopt_ref) {
    // nop
  }

  intrusive_cow_ptr& operator=(intrusive_cow_ptr&&) noexcept = default;

  intrusive_cow_ptr& operator=(const intrusive_cow_ptr&) noexcept = default;

  intrusive_cow_ptr& operator=(counting_pointer x) noexcept {
    ptr_ = std::move(x);
    return *this;
  }

  template <class U = T, class... Ts>
  void emplace(Ts&&... xs) {
    reset(new U(std::forward<Ts>(xs)...), adopt_ref);
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

  void reset() noexcept {
    ptr_.reset();
  }

  /// Replaces the managed object.
  CAF_DEPRECATED("use 'reset(ptr, add_ref)' or 'reset(ptr, adopt_ref)' instead")
  void reset(pointer ptr, bool inc_ref_count = true) noexcept {
    ptr_.reset(ptr, inc_ref_count);
  }

  /// Replaces the managed object.
  void reset(pointer ptr, add_ref_t) noexcept {
    ptr_.reset(ptr, add_ref);
  }

  /// Replaces the managed object.
  void reset(pointer ptr, adopt_ref_t) noexcept {
    ptr_.reset(ptr, adopt_ref);
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
    std::ignore = unshared_ptr();
  }

  /// Returns a mutable pointer to the managed object.
  pointer unshared_ptr() {
    if (ptr_ != nullptr) {
      if (ptr_->strong_reference_count() != 1) {
        if constexpr (detail::has_intrusive_cow_ptr_unshare<T>) {
          return intrusive_cow_ptr_unshare(ptr_.ptr_);
        } else {
          ptr_.reset(ptr_->copy(), adopt_ref);
        }
      }
      return ptr_.get();
    }
    return nullptr;
  }

  /// Returns a mutable reference to the managed object.
  reference unshared() {
    CAF_ASSERT(ptr_ != nullptr);
    return *unshared_ptr();
  }

  // -- observers --------------------------------------------------------------

  /// Returns a read-only pointer to the managed object.
  const_pointer get() const noexcept {
    return ptr_.get();
  }

  CAF_DEPRECATED("accessing the internal intrusive_ptr is no longer supported")
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

#ifdef CAF_ENABLE_RTTI
  template <class C>
  intrusive_cow_ptr<C> downcast() const noexcept {
    using res_t = intrusive_cow_ptr<C>;
    return (ptr_) ? res_t{ptr_.template downcast<C>()} : res_t{};
  }
#endif

  template <class C>
  intrusive_cow_ptr<C> upcast() const noexcept {
    using res_t = intrusive_cow_ptr<C>;
    return (ptr_) ? res_t{ptr_.template upcast<C>()} : res_t{};
  }

  // -- factories --------------------------------------------------------------

  /// Constructs an object of type `T` in an `intrusive_cow_ptr`.
  template <class... Ts>
  static intrusive_cow_ptr make(Ts&&... xs) {
    return intrusive_cow_ptr{new T(std::forward<Ts>(xs)...), adopt_ref};
  }

private:
  // -- member variables -------------------------------------------------------

  counting_pointer ptr_;
};

/// @relates intrusive_cow_ptr
template <class T>
std::string to_string(const intrusive_cow_ptr<T>& x) {
  std::string result;
  detail::append_hex(result, reinterpret_cast<uintptr_t>(x.get()));
  return result;
}

} // namespace caf
