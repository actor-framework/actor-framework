/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ACTOR_HPP
#define CAF_ACTOR_HPP

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>


#include "caf/fwd.hpp"
#include "caf/actor_marker.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

struct invalid_actor_t {
  constexpr invalid_actor_t() {
    // nop
  }
};

/// Identifies an invalid {@link actor}.
/// @relates actor
constexpr invalid_actor_t invalid_actor = invalid_actor_t{};

template <class T>
struct is_convertible_to_actor {
  static constexpr bool value =
      ! std::is_base_of<statically_typed_actor_base, T>::value
      && (std::is_base_of<actor_proxy, T>::value
          || std::is_base_of<local_actor, T>::value);
};

template <>
struct is_convertible_to_actor<scoped_actor> : std::true_type {
  // nop
};

/// Identifies an untyped actor. Can be used with derived types
/// of `event_based_actor`, `blocking_actor`, and `actor_proxy`.
class actor : detail::comparable<actor>,
              detail::comparable<actor, actor_addr>,
              detail::comparable<actor, invalid_actor_t>,
              detail::comparable<actor, invalid_actor_addr_t> {
public:
  // grant access to private ctor
  friend class local_actor;

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

  actor(const scoped_actor&);
  actor(const invalid_actor_t&);

  template <class T,
            class = typename std::enable_if<
                      std::is_base_of<dynamically_typed_actor_base, T>::value
                    >::type>
  actor(T* ptr) : ptr_(ptr->ctrl()) {
    // nop
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

  actor& operator=(const scoped_actor& x);
  actor& operator=(const invalid_actor_t&);

  /// Returns the address of the stored actor.
  actor_addr address() const noexcept;

  /// Returns `*this != invalid_actor`.
  inline operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  /// Returns `*this == invalid_actor`.
  inline bool operator!() const noexcept {
    return ! ptr_;
  }

  /// Returns the origin node of this actor.
  node_id node() const noexcept;

  /// Returns the ID of this actor.
  actor_id id() const noexcept;

  /// Exchange content of `*this` and `other`.
  void swap(actor& other) noexcept;

  /// Create a new actor decorator that presets or reorders inputs.
  template <class... Ts>
  actor bind(Ts&&... xs) const {
    return bind_impl(make_message(std::forward<Ts>(xs)...));
  }

  /// @cond PRIVATE

  inline abstract_actor* operator->() const noexcept {
    CAF_ASSERT(ptr_);
    return ptr_->get();
  }

  intptr_t compare(const actor&) const noexcept;

  intptr_t compare(const actor_addr&) const noexcept;

  inline intptr_t compare(const invalid_actor_t&) const noexcept {
    return ptr_ ? 1 : 0;
  }

  inline intptr_t compare(const invalid_actor_addr_t&) const noexcept {
    return compare(invalid_actor);
  }

  static actor splice_impl(std::initializer_list<actor> xs);

  actor(actor_control_block*, bool);

  template <class Processor>
  friend void serialize(Processor& proc, actor& x, const unsigned int v) {
    serialize(proc, x.ptr_, v);
  }

  friend inline std::string to_string(const actor& x) {
    return to_string(x.ptr_);
  }

  /// @endcond

private:
  actor bind_impl(message msg) const;

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
    return ref ? static_cast<size_t>(ref->id()) : 0;
  }

};
} // namespace std

#endif // CAF_ACTOR_HPP
