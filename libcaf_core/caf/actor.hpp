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

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "caf/config.hpp"

#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/actor_marker.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

template <class T>
struct is_convertible_to_actor {
  static constexpr bool value =
      !std::is_base_of<statically_typed_actor_base, T>::value
      && (std::is_base_of<actor_proxy, T>::value
          || std::is_base_of<local_actor, T>::value);
};

template <>
struct is_convertible_to_actor<scoped_actor> : std::true_type {
  // nop
};

template <class T>
struct is_convertible_to_actor<T*> : is_convertible_to_actor<T> {};

/// Identifies an untyped actor. Can be used with derived types
/// of `event_based_actor`, `blocking_actor`, and `actor_proxy`.
class actor : detail::comparable<actor>,
              detail::comparable<actor, actor_addr>,
              detail::comparable<actor, strong_actor_ptr> {
public:
  // -- friend types that need access to private ctors
  friend class local_actor;

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
            class = typename std::enable_if<
                      std::is_base_of<dynamically_typed_actor_base, T>::value
                    >::type>
  actor(T* ptr) : ptr_(ptr->ctrl()) {
    CAF_ASSERT(ptr != nullptr);
  }

  template <class T>
  typename std::enable_if<is_convertible_to_actor<T>::value, actor&>::type
  operator=(intrusive_ptr<T> ptr) {
    actor tmp{std::move(ptr)};
    swap(tmp);
    return *this;
  }

  template <class T>
  typename std::enable_if<is_convertible_to_actor<T>::value, actor&>::type
  operator=(T* ptr) {
    actor tmp{ptr};
    swap(tmp);
    return *this;
  }

  actor& operator=(std::nullptr_t);

  actor& operator=(const scoped_actor& x);

  /// Queries whether this actor handle is valid.
  inline explicit operator bool() const {
    return static_cast<bool>(ptr_);
  }

  /// Queries whether this actor handle is invalid.
  inline bool operator!() const {
    return !ptr_;
  }

  /// Returns the address of the stored actor.
  actor_addr address() const noexcept;

  /// Returns the ID of this actor.
  inline actor_id id() const noexcept {
    return ptr_->id();
  }

  /// Returns the origin node of this actor.
  inline node_id node() const noexcept {
    return ptr_->node();
  }

  /// Returns the hosting actor system.
  inline actor_system& home_system() const noexcept {
    return *ptr_->home_system;
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor& other) noexcept;

  /// @cond PRIVATE

  inline abstract_actor* operator->() const noexcept {
    CAF_ASSERT(ptr_);
    return ptr_->get();
  }

  intptr_t compare(const actor&) const noexcept;

  intptr_t compare(const actor_addr&) const noexcept;

  intptr_t compare(const strong_actor_ptr&) const noexcept;

  static actor splice_impl(std::initializer_list<actor> xs);

  actor(actor_control_block*, bool);

  /// @endcond

  friend inline std::string to_string(const actor& x) {
    return to_string(x.ptr_);
  }

  friend inline void append_to_string(std::string& x, const actor& y) {
    return append_to_string(x, y.ptr_);
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, actor& x) {
    return inspect(f, x.ptr_);
  }

  /// Releases the reference held by handle `x`. Using the
  /// handle after invalidating it is undefined behavior.
  friend void destroy(actor& x) {
    x.ptr_.reset();
  }

private:
  inline actor_control_block* get() const noexcept {
    return ptr_.get();
  }

  inline actor_control_block* release() noexcept {
    return ptr_.release();
  }

  actor(actor_control_block*);

  strong_actor_ptr ptr_;
};

/// Combine `f` and `g` so that `(f*g)(x) = f(g(x))`.
actor operator*(actor f, actor g);

/// @relates actor
template <class... Ts>
actor splice(const actor& x, const actor& y, const Ts&... zs) {
  return actor::splice_impl({x, y, zs...});
}

/// @relates actor
bool operator==(const actor& lhs, abstract_actor* rhs);

/// @relates actor
bool operator==(abstract_actor* lhs, const actor& rhs);

/// @relates actor
bool operator!=(const actor& lhs, abstract_actor* rhs);

/// @relates actor
bool operator!=(abstract_actor* lhs, const actor& rhs);

} // namespace caf

// allow actor to be used in hash maps
namespace std {
template <>
struct hash<caf::actor> {
  inline size_t operator()(const caf::actor& ref) const {
    return static_cast<size_t>(ref ? ref->id() : 0);
  }
};
} // namespace std

