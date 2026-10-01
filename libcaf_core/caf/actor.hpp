// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_traits.hpp"
#include "caf/add_ref.hpp"
#include "caf/adopt_ref.hpp"
#include "caf/caf_deprecated.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_predicates.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace caf {

/// Identifies an untyped actor. Can be used with derived types
/// of `event_based_actor`, `blocking_actor`, and `actor_proxy`.
class CAF_CORE_EXPORT actor {
public:
  // -- friends ----------------------------------------------------------------

  friend class local_actor;
  friend class abstract_actor;

  template <class>
  friend struct detail::with_actor_addr_from;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  using signatures = none_t;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  constexpr actor() noexcept = default;

  constexpr actor(std::nullptr_t) noexcept {
    // nop
  }

  actor(const scoped_actor&) noexcept;

  template <class T>
    requires actor_traits<T>::is_dynamically_typed
  actor(T* ptr) : ptr_(ptr->ctrl(), add_ref) {
    CAF_ASSERT(ptr != nullptr);
  }

  template <class T>
    requires actor_traits<T>::is_dynamically_typed
  actor& operator=(intrusive_ptr<T> ptr) {
    actor tmp{std::move(ptr)};
    swap(tmp);
    return *this;
  }

  template <class T>
    requires actor_traits<T>::is_dynamically_typed
  actor& operator=(T* ptr) {
    actor tmp{ptr};
    swap(tmp);
    return *this;
  }

  actor& operator=(std::nullptr_t);

  actor& operator=(const scoped_actor& x);

  /// Queries whether this actor handle is valid.
  explicit operator bool() const {
    return static_cast<bool>(ptr_);
  }

  /// Queries whether this actor handle is invalid.
  bool operator!() const {
    return !ptr_;
  }

  /// Returns the stored strong actor pointer.
  const strong_actor_ptr& as_intrusive_ptr() const noexcept {
    return ptr_;
  }

  /// Returns the address of the stored actor.
  actor_addr address() const noexcept;

  /// Returns the ID of this actor.
  actor_id id() const noexcept {
    return ptr_->id();
  }

  /// Returns the origin node of this actor.
  node_id node() const noexcept {
    return ptr_->node();
  }

  /// Returns the hosting actor system.
  actor_system& home_system() const noexcept {
    return ptr_->system();
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor& other) noexcept;

  /// @cond

  abstract_actor* operator->() const noexcept {
    CAF_ASSERT(ptr_);
    return ptr_->managed();
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  actor(actor_control_block*, bool);

  actor(actor_control_block* ptr, add_ref_t) noexcept : ptr_(ptr, add_ref) {
    // nop
  }

  actor(actor_control_block* ptr, adopt_ref_t) noexcept : ptr_(ptr, adopt_ref) {
    // nop
  }

  /// @endcond

  friend std::string to_string(const actor& x) {
    return to_string(x.ptr_);
  }

  friend void append_to_string(std::string& x, const actor& y) {
    return append_to_string(x, y.ptr_);
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, actor& x) {
    return f.value(x.ptr_);
  }

  /// Releases the reference held by handle `x`. Using the
  /// handle after invalidating it is undefined behavior.
  friend void destroy(actor& x) {
    x.ptr_.reset();
  }

private:
  actor_control_block* get() const noexcept {
    return ptr_.get();
  }

  actor_control_block* release() noexcept {
    return ptr_.release();
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  explicit actor(actor_control_block*) noexcept;

  strong_actor_ptr ptr_;
};

// Note: `actor` allows implicit conversions from pointers. Simply defaulting
//       `operator<=>` would result in side effects when comparing actors and
//       pointers (increasing and then decreasing the reference count). Hence,
//       we implement comparison manually here in a way that blocks implicit
//       conversions. Since this code would be the same for `actor` and
//       `typed_actor` anyways (and we have to use templates regardless), we
//       implement comparison for both types here (typed_actor.hpp includes this
//       header). Comparison to `actor_addr` is enabled by specializing
//       `with_actor_addr_from`.

template <detail::actor_handle Handle>
bool operator==(const Handle& hdl, std::nullptr_t) noexcept {
  return !hdl;
}

template <detail::actor_handle Handle>
bool operator!=(const Handle& hdl, std::nullptr_t) noexcept {
  return static_cast<bool>(hdl);
}

template <detail::actor_handle Handle>
bool operator==(std::nullptr_t, const Handle& hdl) noexcept {
  return !hdl;
}

template <detail::actor_handle Handle>
bool operator!=(std::nullptr_t, const Handle& hdl) noexcept {
  return static_cast<bool>(hdl);
}

template <detail::actor_handle Left, detail::actor_handle Right>
bool operator==(const Left& lhs, const Right& rhs) noexcept {
  return lhs.as_intrusive_ptr() == rhs.as_intrusive_ptr();
}

template <detail::actor_handle Left, detail::actor_handle Right>
auto operator<=>(const Left& lhs, const Right& rhs) noexcept {
  return lhs.as_intrusive_ptr() <=> rhs.as_intrusive_ptr();
}

template <detail::actor_handle Left>
bool operator==(const Left& lhs, const strong_actor_ptr& rhs) noexcept {
  return lhs.as_intrusive_ptr() == rhs;
}

template <detail::actor_handle Left>
auto operator<=>(const Left& lhs, const strong_actor_ptr& rhs) noexcept {
  return lhs.as_intrusive_ptr() <=> rhs;
}

template <detail::actor_handle Right>
bool operator==(const strong_actor_ptr& lhs, const Right& rhs) noexcept {
  return lhs == rhs.as_intrusive_ptr();
}

template <detail::actor_handle Right>
auto operator<=>(const strong_actor_ptr& lhs, const Right& rhs) noexcept {
  return lhs <=> rhs.as_intrusive_ptr();
}

template <detail::actor_handle Left, std::derived_from<abstract_actor> Right>
bool operator==(const Left& lhs, const Right* rhs) noexcept {
  return lhs.as_intrusive_ptr() == actor_control_block::from(rhs);
}

template <detail::actor_handle Left, std::derived_from<abstract_actor> Right>
auto operator<=>(const Left& lhs, const Right* rhs) noexcept {
  return lhs.as_intrusive_ptr() <=> actor_control_block::from(rhs);
}

template <std::derived_from<abstract_actor> Left, detail::actor_handle Right>
bool operator==(const Left* lhs, const Right& rhs) noexcept {
  return actor_control_block::from(lhs) == rhs.as_intrusive_ptr();
}

template <std::derived_from<abstract_actor> Left, detail::actor_handle Right>
auto operator<=>(const Left* lhs, const Right& rhs) noexcept {
  return actor_control_block::from(lhs) <=> rhs.as_intrusive_ptr();
}

template <detail::actor_handle Left>
bool operator==(const Left& lhs, const actor_control_block* rhs) noexcept {
  return lhs.as_intrusive_ptr() == rhs;
}

template <detail::actor_handle Left>
auto operator<=>(const Left& lhs, const actor_control_block* rhs) noexcept {
  return lhs.as_intrusive_ptr() <=> rhs;
}

template <detail::actor_handle Right>
bool operator==(const actor_control_block* lhs, const Right& rhs) noexcept {
  return lhs == rhs.as_intrusive_ptr();
}

template <detail::actor_handle Right>
auto operator<=>(const actor_control_block* lhs, const Right& rhs) noexcept {
  return lhs <=> rhs.as_intrusive_ptr();
}

} // namespace caf

namespace caf::detail {

/// Customization point for enabling comparison between actor_addr and `Handle`.
template <>
struct with_actor_addr_from<actor> {
  static constexpr bool specialized = true;

  template <class Visitor>
  static auto visit(const actor& hdl, Visitor&& visitor) {
    auto addr = hdl.address();
    return std::forward<Visitor>(visitor)(addr);
  }
};

} // namespace caf::detail

namespace std {

template <>
struct hash<caf::actor> {
  size_t operator()(const caf::actor& ref) const noexcept {
    if (!ref) {
      return 0;
    }
    return caf::hash::fnv<size_t>::compute(ref.id(), ref.node());
  }
};

} // namespace std
