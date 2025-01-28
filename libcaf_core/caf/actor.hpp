// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_traits.hpp"
#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace caf {

/// Identifies an untyped actor. Can be used with derived types
/// of `event_based_actor`, `blocking_actor`, and `actor_proxy`.
class CAF_CORE_EXPORT actor : detail::comparable<actor>,
                              detail::comparable<actor, actor_addr>,
                              detail::comparable<actor, strong_actor_ptr> {
public:
  // -- friends ----------------------------------------------------------------

  friend class local_actor;
  friend class abstract_actor;

  using signatures = none_t;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  actor() = default;
  actor(actor&&) = default;
  actor(const actor&) = default;
  actor& operator=(actor&&) = default;
  actor& operator=(const actor&) = default;

  actor(std::nullptr_t);

  actor(const scoped_actor&);

  template <class T,
            class = std::enable_if_t<actor_traits<T>::is_dynamically_typed>>
  actor(T* ptr) : ptr_(ptr->ctrl()) {
    CAF_ASSERT(ptr != nullptr);
  }

  template <class T,
            class = std::enable_if_t<actor_traits<T>::is_dynamically_typed>>
  actor& operator=(intrusive_ptr<T> ptr) {
    actor tmp{std::move(ptr)};
    swap(tmp);
    return *this;
  }

  template <class T,
            class = std::enable_if_t<actor_traits<T>::is_dynamically_typed>>
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
    return *ptr_->home_system;
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor& other) noexcept;

  /// @cond

  abstract_actor* operator->() const noexcept {
    CAF_ASSERT(ptr_);
    return ptr_->get();
  }

  intptr_t compare(const actor&) const noexcept;

  intptr_t compare(const actor_addr&) const noexcept;

  intptr_t compare(const strong_actor_ptr&) const noexcept;

  actor(actor_control_block*, bool);

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

  actor(actor_control_block*);

  strong_actor_ptr ptr_;
};

/// @relates actor
CAF_CORE_EXPORT bool operator==(const actor& lhs, abstract_actor* rhs);

/// @relates actor
CAF_CORE_EXPORT bool operator==(abstract_actor* lhs, const actor& rhs);

/// @relates actor
CAF_CORE_EXPORT bool operator!=(const actor& lhs, abstract_actor* rhs);

/// @relates actor
CAF_CORE_EXPORT bool operator!=(abstract_actor* lhs, const actor& rhs);

} // namespace caf

// allow actor to be used in hash maps
namespace std {
template <>
struct hash<caf::actor> {
  size_t operator()(const caf::actor& ref) const noexcept {
    return static_cast<size_t>(ref ? ref->id() : 0);
  }
};
} // namespace std
