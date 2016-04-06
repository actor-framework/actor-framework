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

#include "caf/detail/comparable.hpp"

namespace caf {

struct invalid_actor_addr_t {
  constexpr invalid_actor_addr_t() {}

};

/// Identifies an invalid {@link actor_addr}.
/// @relates actor_addr
constexpr invalid_actor_addr_t invalid_actor_addr = invalid_actor_addr_t{};

/// Stores the address of typed as well as untyped actors.
class actor_addr : detail::comparable<actor_addr>,
                   detail::comparable<actor_addr, weak_actor_ptr>,
                   detail::comparable<actor_addr, strong_actor_ptr>,
                   detail::comparable<actor_addr, abstract_actor*>,
                   detail::comparable<actor_addr, actor_control_block*> {
public:
  // grant access to private ctor
  friend class actor;
  friend class abstract_actor;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = true;

  actor_addr() = default;
  actor_addr(actor_addr&&) = default;
  actor_addr(const actor_addr&) = default;
  actor_addr& operator=(actor_addr&&) = default;
  actor_addr& operator=(const actor_addr&) = default;

  actor_addr(const invalid_actor_addr_t&);

  actor_addr operator=(const invalid_actor_addr_t&);

  /// Returns `*this != invalid_actor_addr`.
  inline explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  /// Returns `*this == invalid_actor_addr`.
  inline bool operator!() const noexcept {
    return !ptr_;
  }

  /// Returns the ID of this actor.
  actor_id id() const noexcept;

  /// Returns the origin node of this actor.
  node_id node() const noexcept;

  /// Returns the hosting actor system.
  actor_system* home_system() const noexcept;

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

  template <class Processor>
  friend void serialize(Processor& proc, actor_addr& x, const unsigned int v) {
    serialize(proc, x.ptr_, v);
  }

  friend inline std::string to_string(const actor_addr& x) {
    return to_string(x.ptr_);
  }

  actor_addr(actor_control_block*, bool);

  /// @endcond

private:
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
