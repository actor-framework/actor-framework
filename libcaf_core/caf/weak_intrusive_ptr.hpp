// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace caf::detail {

template <class ControlBlock, class Pointer>
ControlBlock* get_control_block(const Pointer& ptr) noexcept {
  if constexpr (std::is_same_v<ControlBlock, typename Pointer::element_type>) {
    return ptr.get();
  } else {
    if (ptr) {
      return ptr->ctrl();
    }
    return nullptr;
  }
}

template <class ControlBlock, class T>
struct managed_by_impl {
  static constexpr bool value = requires(T& obj) {
    { obj.ctrl() } -> std::same_as<ControlBlock*>;
  };
};

template <class ControlBlock>
struct managed_by_impl<ControlBlock, ControlBlock> {
  static constexpr bool value = true;
};

template <class ControlBlock, class T>
concept managed_by = managed_by_impl<ControlBlock, T>::value;

template <class Left, class Right>
inline constexpr bool same_control_block
  = std::is_same_v<typename weak_intrusive_ptr<Left>::control_block_type,
                   typename weak_intrusive_ptr<Right>::control_block_type>;

template <class T>
using control_block_of =
  typename weak_intrusive_ptr_traits<T>::control_block_type;

} // namespace caf::detail

namespace caf {

/// A smart pointer that holds a non-owning reference to an object. A weak
/// pointer always points to the control block of an object, never to the object
/// itself. The template parameter `T` may be either a managed type or a control
/// block type.
///
/// The control block type must be tied to the managed type. For polymorphic
/// types, the control block must be tied to the base type of the type
/// hierarchy, i.e., calling `managed()` on the control block must return a
/// pointer to the base type.
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

  /// Tells `actor_cast` which semantic this type uses.
  static constexpr bool has_weak_ptr_semantics = true;

  /// Whether the template parameter `T` is the control block type.
  static constexpr bool has_control_block_type
    = std::is_same_v<T, control_block_type>;

  /// Whether the template parameter `T` is the control block type or the base
  /// type of the type hierarchy.
  static constexpr bool has_base_type = has_control_block_type
                                        || std::is_same_v<T, managed_type>;

  constexpr weak_intrusive_ptr() noexcept : ptr_(nullptr) {
    // nop
  }

  explicit weak_intrusive_ptr(
    const intrusive_ptr<control_block_type>& src) noexcept
    requires has_base_type
  {
    if (src) {
      ptr_ = src.get();
      ptr_->ref_weak();
    } else {
      ptr_ = nullptr;
    }
  }

  template <std::derived_from<managed_type> U>
  explicit weak_intrusive_ptr(const intrusive_ptr<U>& src) noexcept {
    if (src) {
      ptr_ = src->ctrl();
      ptr_->ref_weak();
    } else {
      ptr_ = nullptr;
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
    : ptr_(other.release()) {
    // nop
  }

  weak_intrusive_ptr(const weak_intrusive_ptr& other) noexcept
    : ptr_(other.ptr_) {
    if (ptr_) {
      ptr_->ref_weak();
    }
  }

  template <std::same_as<control_block_type> U>
    requires has_base_type
  weak_intrusive_ptr(weak_intrusive_ptr<U> src) noexcept : ptr_(src.release()) {
    // nop
  }

  template <std::derived_from<managed_type> U>
  weak_intrusive_ptr(weak_intrusive_ptr<U> src) noexcept : ptr_(src.release()) {
    // nop
  }

  ~weak_intrusive_ptr() {
    if (ptr_) {
      ptr_->deref_weak();
    }
  }

  void swap(weak_intrusive_ptr& other) noexcept {
    auto* tmp = ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = tmp;
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

  weak_intrusive_ptr&
  operator=(const intrusive_ptr<control_block_type>& src) noexcept
    requires has_base_type
  {
    weak_intrusive_ptr tmp{src};
    swap(tmp);
    return *this;
  }

  template <std::derived_from<managed_type> U>
  weak_intrusive_ptr& operator=(const intrusive_ptr<U>& src) noexcept {
    weak_intrusive_ptr tmp{src};
    swap(tmp);
    return *this;
  }

  template <std::same_as<control_block_type> U>
  weak_intrusive_ptr& operator=(weak_intrusive_ptr<U> src) noexcept
    requires has_base_type
  {
    swap(src);
    return *this;
  }

  template <std::derived_from<managed_type> U>
  weak_intrusive_ptr& operator=(weak_intrusive_ptr<U> src) noexcept {
    swap(src);
    return *this;
  }

  ptrdiff_t compare(const weak_intrusive_ptr& other) const noexcept {
    if (ptr_ < other.ptr_) {
      return -1;
    }
    if (ptr_ > other.ptr_) {
      return 1;
    }
    return 0;
  }

  template <class U>
    requires detail::managed_by<control_block_type, U>
  ptrdiff_t compare(const intrusive_ptr<U>& other) const noexcept {
    auto* ctrl = detail::get_control_block<control_block_type>(other);
    if (ptr_ < ctrl) {
      return -1;
    }
    if (ptr_ > ctrl) {
      return 1;
    }
    return 0;
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
      return {ptr_->managed(), adopt_ref};
    }
  }

  /// Returns the raw pointer without modifying reference
  /// count and sets this to `nullptr`.
  [[nodiscard]] control_block_pointer release() noexcept {
    if (ptr_ != nullptr) {
      auto result = ptr_;
      ptr_ = nullptr;
      return result;
    }
    return nullptr;
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
  auto* detach() noexcept {
    return release();
  }

  CAF_DEPRECATED("use lock() instead")
  pointer get_locked() const noexcept {
    if (!ptr_ || !ptr_->upgrade_weak())
      return nullptr;
    return ptr_->managed();
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
  control_block_pointer ptr_;
};

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator==(const weak_intrusive_ptr<T>& lhs, std::nullptr_t) noexcept {
  return !lhs;
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator==(std::nullptr_t, const weak_intrusive_ptr<T>& rhs) noexcept {
  return !rhs;
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator!=(const weak_intrusive_ptr<T>& lhs, std::nullptr_t) noexcept {
  return static_cast<bool>(lhs);
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator!=(std::nullptr_t, const weak_intrusive_ptr<T>& rhs) noexcept {
  return static_cast<bool>(rhs);
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator==(const weak_intrusive_ptr<T>& lhs,
           typename weak_intrusive_ptr<T>::control_block_type* rhs) noexcept {
  return lhs.ctrl() == rhs;
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator==(typename weak_intrusive_ptr<T>::control_block_type* lhs,
           const weak_intrusive_ptr<T>& rhs) noexcept {
  return lhs == rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator!=(const weak_intrusive_ptr<T>& lhs,
           typename weak_intrusive_ptr<T>::control_block_type* rhs) noexcept {
  return lhs.ctrl() != rhs;
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator!=(typename weak_intrusive_ptr<T>::control_block_type* lhs,
           const weak_intrusive_ptr<T>& rhs) noexcept {
  return lhs != rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator<(const weak_intrusive_ptr<T>& lhs,
          typename weak_intrusive_ptr<T>::control_block_type* rhs) noexcept {
  return lhs.ctrl() < rhs;
}

/// @relates weak_intrusive_ptr
template <class T>
constexpr bool
operator<(typename weak_intrusive_ptr<T>::control_block_type* lhs,
          const weak_intrusive_ptr<T>& rhs) noexcept {
  return lhs < rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::same_control_block<Left, Right>
constexpr bool operator==(const weak_intrusive_ptr<Left>& lhs,
                          const weak_intrusive_ptr<Right>& rhs) noexcept {
  return lhs.ctrl() == rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::same_control_block<Left, Right>
constexpr bool operator!=(const weak_intrusive_ptr<Left>& lhs,
                          const weak_intrusive_ptr<Right>& rhs) noexcept {
  return lhs.ctrl() != rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::same_control_block<Left, Right>
constexpr bool operator<(const weak_intrusive_ptr<Left>& lhs,
                         const weak_intrusive_ptr<Right>& rhs) noexcept {
  return lhs.ctrl() < rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::managed_by<detail::control_block_of<Left>, Right>
constexpr bool operator==(const weak_intrusive_ptr<Left>& lhs,
                          const intrusive_ptr<Right>& rhs) noexcept {
  return lhs.ctrl()
         == detail::get_control_block<detail::control_block_of<Left>>(rhs);
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::managed_by<detail::control_block_of<Right>, Left>
constexpr bool operator==(const intrusive_ptr<Left>& lhs,
                          const weak_intrusive_ptr<Right>& rhs) noexcept {
  return detail::get_control_block<detail::control_block_of<Right>>(lhs)
         == rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::managed_by<detail::control_block_of<Left>, Right>
constexpr bool operator!=(const weak_intrusive_ptr<Left>& lhs,
                          const intrusive_ptr<Right>& rhs) noexcept {
  return lhs.ctrl()
         != detail::get_control_block<detail::control_block_of<Left>>(rhs);
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::managed_by<detail::control_block_of<Right>, Left>
constexpr bool operator!=(const intrusive_ptr<Left>& lhs,
                          const weak_intrusive_ptr<Right>& rhs) noexcept {
  return detail::get_control_block<detail::control_block_of<Right>>(lhs)
         != rhs.ctrl();
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::managed_by<detail::control_block_of<Left>, Right>
constexpr bool operator<(const weak_intrusive_ptr<Left>& lhs,
                         const intrusive_ptr<Right>& rhs) noexcept {
  return lhs.ctrl()
         < detail::get_control_block<detail::control_block_of<Left>>(rhs);
}

/// @relates weak_intrusive_ptr
template <class Left, class Right>
  requires detail::managed_by<detail::control_block_of<Right>, Left>
constexpr bool operator<(const intrusive_ptr<Left>& lhs,
                         const weak_intrusive_ptr<Right>& rhs) noexcept {
  return detail::get_control_block<detail::control_block_of<Right>>(lhs)
         < rhs.ctrl();
}

/// @relates weak_intrusive_ptr
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
