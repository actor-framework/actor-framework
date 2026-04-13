// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/control_block_traits.hpp"
#include "caf/intrusive_ptr.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace caf::detail {

template <class T, class Managed, class ControlBlock, class Element>
struct weak_assignable_from_oracle {
  // an intrusive_ptr<Element> cannot assign from a base type of Element; hence,
  // we also cannot allow assignment from a ControlBlock since that might point
  // to a base type of Element
  static constexpr bool value = std::is_base_of_v<Element, T>;
};

template <class T, class Managed, class ControlBlock>
struct weak_assignable_from_oracle<T, Managed, ControlBlock, Managed> {
  // an intrusive_ptr<Managed> allows assignment from ControlBlock, since the
  // control block must, by definition, point to a managed object that is either
  // Managed or a derived type of Managed
  static constexpr bool value = std::is_same_v<T, ControlBlock>
                                || std::is_base_of_v<Managed, T>;
};

template <class T, class Managed, class ControlBlock>
struct weak_assignable_from_oracle<T, Managed, ControlBlock, ControlBlock> {
  // an intrusive_ptr<ControlBlock> behaves like a weak_intrusive_ptr<Managed>
  static constexpr bool value = std::is_same_v<T, ControlBlock>
                                || std::is_base_of_v<Managed, T>;
};

template <class T, class Managed, class ControlBlock, class Element>
concept weak_assignable_from
  = weak_assignable_from_oracle<T, Managed, ControlBlock, Element>::value;

template <class T, class Managed, class ControlBlock>
concept weak_comparable_with = std::is_same_v<T, ControlBlock>
                               || std::is_base_of_v<Managed, T>;

} // namespace caf::detail

namespace caf {

/// A smart pointer that holds a non-owning reference to an object. A weak
/// pointer always points to the control block of an object, never to the object
/// itself. The template parameter `T` may be either a managed type or a control
/// block type.
/// @note `weak_intrusive_ptr_traits<T>` *must* be specialized.
template <class T>
class weak_intrusive_ptr {
public:
  template <class>
  friend class weak_intrusive_ptr;

  using traits = weak_intrusive_ptr_traits<T>;

  using element_type = T;

  using pointer = element_type*;

  using const_pointer = const element_type*;

  using reference = element_type&;

  using const_reference = const element_type&;

  using managed_type = typename traits::managed_type;

  using control_block_type = typename traits::control_block_type;

  using control_block_pointer = control_block_type*;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = true;

  constexpr weak_intrusive_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  template <detail::weak_assignable_from<managed_type, control_block_type, T> U>
  explicit weak_intrusive_ptr(const intrusive_ptr<U>& src) noexcept
    : ptr_(ctrl_ptr(src.get())) {
    if (ptr_) {
      ptr_->ref_weak();
    }
  }

  weak_intrusive_ptr(control_block_pointer ptr, add_ref_t) noexcept
    : ptr_(ptr) {
    if (ptr_) {
      ptr_->ref_weak();
    }
  }

  constexpr weak_intrusive_ptr(control_block_pointer ptr, adopt_ref_t) noexcept
    : ptr_(ptr) {
    // nop
  }

  weak_intrusive_ptr(weak_intrusive_ptr&& other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr)) {
    // nop
  }

  weak_intrusive_ptr(const weak_intrusive_ptr& other) noexcept
    : ptr_(other.ptr_) {
    if (ptr_) {
      ptr_->ref_weak();
    }
  }

  template <detail::weak_assignable_from<managed_type, control_block_type, T> U>
  weak_intrusive_ptr(weak_intrusive_ptr<U> other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr)) {
    // nop
  }

  ~weak_intrusive_ptr() {
    if (ptr_) {
      ptr_->deref_weak();
    }
  }

  void swap(weak_intrusive_ptr& other) noexcept {
    std::swap(ptr_, other.ptr_);
  }

  void reset() noexcept {
    if (ptr_) {
      // Must set ptr_ to nullptr BEFORE calling release, because release may
      // trigger destruction of an object that owns this weak_intrusive_ptr. If
      // ptr_ is still set when the owner's destructor runs, it would try to
      // release again, causing a double-free.
      auto tmp = ptr_;
      ptr_ = nullptr;
      tmp->deref_weak();
    }
  }

  weak_intrusive_ptr& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  weak_intrusive_ptr& operator=(weak_intrusive_ptr&& other) noexcept {
    swap(other);
    return *this;
  }

  weak_intrusive_ptr& operator=(const weak_intrusive_ptr& other) noexcept {
    weak_intrusive_ptr tmp{other};
    swap(tmp);
    return *this;
  }

  template <detail::weak_assignable_from<managed_type, control_block_type, T> U>
  weak_intrusive_ptr& operator=(const intrusive_ptr<U>& src) noexcept {
    weak_intrusive_ptr tmp{src};
    swap(tmp);
    return *this;
  }

  template <detail::weak_assignable_from<managed_type, control_block_type, T> U>
  weak_intrusive_ptr& operator=(weak_intrusive_ptr<U> src) noexcept {
    swap(src);
    return *this;
  }

  /// Returns a pointer to the control block.
  control_block_pointer ctrl() const noexcept {
    return ptr_;
  }

  bool operator!() const noexcept {
    return !ptr_;
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  constexpr ptrdiff_t compare(control_block_pointer ptr) const noexcept {
    return static_cast<ptrdiff_t>(ptr_ - ptr);
  }

  template <detail::weak_comparable_with<managed_type, control_block_type> U>
  ptrdiff_t compare(const intrusive_ptr<U>& other) const noexcept {
    if (other) {
      return static_cast<ptrdiff_t>(ptr_ - ctrl_ptr(other.get()));
    }
    return compare(nullptr);
  }

  template <detail::weak_comparable_with<managed_type, control_block_type> U>
  ptrdiff_t compare(const weak_intrusive_ptr<U>& other) const noexcept {
    return compare(other.ctrl());
  }

  ptrdiff_t compare(std::nullptr_t) const noexcept {
    return reinterpret_cast<ptrdiff_t>(ptr_);
  }

  size_t hash() const noexcept {
    std::hash<control_block_pointer> hasher;
    return hasher(ptr_);
  }

  /// Tries to upgrade this weak reference to a strong reference.
  intrusive_ptr<T> lock() const noexcept {
    if (!ptr_ || !ptr_->upgrade_weak()) {
      return nullptr;
    }
    // Note: reference count already increased by do_upgrade_weak.
    if constexpr (std::is_same_v<control_block_type, T>) {
      return {ptr_, adopt_ref};
    } else {
      return {get_ptr(ptr_), adopt_ref};
    }
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  weak_intrusive_ptr(control_block_pointer ptr,
                     bool increase_ref_count = true) noexcept
    : ptr_(ptr) {
    if (ptr_ && increase_ref_count) {
      ptr_->ref_weak();
    }
  }

  CAF_DEPRECATED("no longer supported")
  pointer release() noexcept {
    if (auto* result = ptr_; result) {
      ptr_ = nullptr;
      return get_ptr(result);
    }
    return nullptr;
  }

  CAF_DEPRECATED("no longer supported")
  pointer detach() noexcept {
    return release();
  }

  CAF_DEPRECATED("use lock() instead")
  pointer get_locked() const noexcept {
    if (!ptr_ || !ptr_->upgrade_weak())
      return nullptr;
    return get_ptr(ptr_);
  }

  CAF_DEPRECATED("no longer supported")
  auto* get() const noexcept
    requires std::is_same_v<control_block_type, element_type>
  {
    return ptr_;
  }

  CAF_DEPRECATED("no longer supported")
  auto* operator->() const noexcept
    requires std::is_same_v<control_block_type, element_type>
  {
    return ptr_;
  }

  CAF_DEPRECATED("no longer supported")
  auto& operator*() const noexcept
    requires std::is_same_v<control_block_type, element_type>
  {
    return *ptr_;
  }

private:
  using control_block_traits = detail::control_block_traits<control_block_type>;

  template <class U>
  static control_block_pointer ctrl_ptr(U* ptr) noexcept {
    if constexpr (std::is_same_v<control_block_type, U>) {
      return ptr;
    } else {
      return control_block_traits::ctrl_ptr(ptr);
    }
  }

  static pointer get_ptr(control_block_pointer ptr) noexcept {
    if constexpr (std::is_same_v<control_block_type, element_type>) {
      return ptr;
    } else {
      using managed_type = typename control_block_type::managed_type;
      auto* mptr = control_block_traits::managed_ptr(ptr);
      if constexpr (std::is_same_v<managed_type, element_type>) {
        return mptr;
      } else {
        static_assert(std::is_base_of_v<managed_type, element_type>);
        return static_cast<T*>(mptr);
      }
    }
  }

  void set_ptr(control_block_pointer ptr, bool increase_ref_count) noexcept {
    ptr_ = ptr;
    if (ptr && increase_ref_count) {
      ptr->ref_weak();
    }
  }

  control_block_pointer ptr_;
};

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator==(const weak_intrusive_ptr<Lhs>& lhs,
                const weak_intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Lhs>,
                                        weak_intrusive_ptr<Rhs>>
{
  return lhs.compare(rhs) == 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator<(const weak_intrusive_ptr<Lhs>& lhs,
               const weak_intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Lhs>,
                                        weak_intrusive_ptr<Rhs>>
{
  return lhs.compare(rhs) < 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator!=(const weak_intrusive_ptr<Lhs>& lhs,
                const weak_intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Lhs>,
                                        weak_intrusive_ptr<Rhs>>
{
  return lhs.compare(rhs) != 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator==(const weak_intrusive_ptr<Lhs>& lhs,
                const intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Lhs>,
                                        intrusive_ptr<Rhs>>
{
  return lhs.compare(rhs) == 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator<(const weak_intrusive_ptr<Lhs>& lhs,
               const intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Lhs>,
                                        intrusive_ptr<Rhs>>
{
  return lhs.compare(rhs) < 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator!=(const weak_intrusive_ptr<Lhs>& lhs,
                const intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Lhs>,
                                        intrusive_ptr<Rhs>>
{
  return lhs.compare(rhs) != 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator==(const intrusive_ptr<Lhs>& lhs,
                const weak_intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Rhs>,
                                        intrusive_ptr<Lhs>>
{
  return rhs.compare(lhs) == 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator<(const intrusive_ptr<Lhs>& lhs,
               const weak_intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Rhs>,
                                        intrusive_ptr<Lhs>>
{
  return rhs.compare(lhs) >= 0;
}

/// @relates weak_intrusive_ptr
template <class Lhs, class Rhs>
bool operator!=(const intrusive_ptr<Lhs>& lhs,
                const weak_intrusive_ptr<Rhs>& rhs)
  requires detail::has_compare_overload<weak_intrusive_ptr<Rhs>,
                                        intrusive_ptr<Lhs>>
{
  return rhs.compare(lhs) != 0;
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator==(
  const weak_intrusive_ptr<T>& lhs,
  const typename weak_intrusive_ptr<T>::control_block_pointer rhs) {
  return lhs.ctrl() == rhs;
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator==(const typename weak_intrusive_ptr<T>::control_block_pointer lhs,
                const weak_intrusive_ptr<T>& rhs) {
  return lhs == rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator!=(
  const weak_intrusive_ptr<T>& lhs,
  const typename weak_intrusive_ptr<T>::control_block_pointer rhs) {
  return !(lhs == rhs);
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator!=(const typename weak_intrusive_ptr<T>::control_block_pointer lhs,
                const weak_intrusive_ptr<T>& rhs) {
  return !(lhs == rhs);
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator==(const weak_intrusive_ptr<T>& lhs, std::nullptr_t) {
  return lhs.compare(nullptr) == 0;
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator==(std::nullptr_t, const weak_intrusive_ptr<T>& rhs) {
  return rhs.compare(nullptr) == 0;
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator!=(const weak_intrusive_ptr<T>& lhs, std::nullptr_t) {
  return lhs.compare(nullptr) != 0;
}

/// @relates weak_intrusive_ptr
template <class T>
bool operator!=(std::nullptr_t, const weak_intrusive_ptr<T>& rhs) {
  return rhs.compare(nullptr) != 0;
}

template <class T>
std::string to_string(const weak_intrusive_ptr<T>& x) {
  std::string result;
  detail::append_hex(result, reinterpret_cast<uintptr_t>(x.ctrl()));
  return result;
}

} // namespace caf

namespace std {

template <class T>
struct hash<caf::weak_intrusive_ptr<T>> {
  size_t operator()(const caf::weak_intrusive_ptr<T>& ptr) const noexcept {
    return ptr.hash();
  }
};

} // namespace std
