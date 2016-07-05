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

#ifndef CAF_ACTOR_ADDR_HPP
#define CAF_ACTOR_ADDR_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/unsafe_actor_handle_init.hpp"

#include "caf/detail/comparable.hpp"

namespace caf {

/// Stores the address of typed as well as untyped actors.
class actor_addr : detail::comparable<actor_addr>,
                   detail::comparable<actor_addr, weak_actor_ptr>,
                   detail::comparable<actor_addr, strong_actor_ptr>,
                   detail::comparable<actor_addr, abstract_actor*>,
                   detail::comparable<actor_addr, actor_control_block*> {
public:
  // -- friend types that need access to private ctors

  template <class>
  friend class detail::type_erased_value_impl;

  template <class>
  friend class data_processor;

  // grant access to private ctor
  friend class actor;
  friend class abstract_actor;
  friend class down_msg;
  friend class exit_msg;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = true;

  // tell actor_cast this is a nullable handle type
  static constexpr bool has_non_null_guarantee = true;

  actor_addr(actor_addr&&) = default;
  actor_addr(const actor_addr&) = default;
  actor_addr& operator=(actor_addr&&) = default;
  actor_addr& operator=(const actor_addr&) = default;

  actor_addr(const unsafe_actor_handle_init_t&);

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
  void swap(actor_addr& other) noexcept;

  /// @cond PRIVATE

  static intptr_t compare(const actor_control_block* lhs,
                          const actor_control_block* rhs);

  intptr_t compare(const actor_addr& other) const noexcept;

  intptr_t compare(const abstract_actor* other) const noexcept;

  intptr_t compare(const actor_control_block* other) const noexcept;

  inline intptr_t compare(const weak_actor_ptr& other) const noexcept {
    return compare(other.get());
  }

  inline intptr_t compare(const strong_actor_ptr& other) const noexcept {
    return compare(other.get());
  }

  template <class Inspector>
  friend error inspect(Inspector& f, actor_addr& x) {
    return inspect(f, x.ptr_);
  }

  /// Releases the reference held by handle `x`. Using the
  /// handle after invalidating it is undefined behavior.
  friend void destroy(actor_addr& x) {
    x.ptr_.reset();
  }

  actor_addr(actor_control_block*, bool);

  /// @endcond

private:
  actor_addr() = default;

  inline actor_control_block* get() const noexcept {
    return ptr_.get();
  }

  inline actor_control_block* release() noexcept {
    return ptr_.release();
  }

  inline actor_control_block* get_locked() const noexcept {
    return ptr_.get_locked();
  }

  actor_addr(actor_control_block*);

  weak_actor_ptr ptr_;
};


} // namespace caf

// allow actor_addr to be used in hash maps
namespace std {
template <>
struct hash<caf::actor_addr> {
  inline size_t operator()(const caf::actor_addr& ref) const {
    return static_cast<size_t>(ref.id());
  }
};
} // namespace std

#endif // CAF_ACTOR_ADDR_HPP
