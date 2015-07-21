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

#include "caf/intrusive_ptr.hpp"

#include "caf/fwd.hpp"
#include "caf/abstract_actor.hpp"

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
                   detail::comparable<actor_addr, abstract_actor*>,
                   detail::comparable<actor_addr, abstract_actor_ptr> {
public:
  // grant access to private ctor
  friend class actor;
  friend class abstract_actor;

  template <class T, typename U>
  friend T actor_cast(const U&);


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

  /// Returns whether this is an handle to a remote actor.
  bool is_remote() const noexcept;

  /// Returns the ID of this actor.
  actor_id id() const noexcept;

  /// Returns the origin node of this actor.
  node_id node() const noexcept;

  /// Exchange content of `*this` and `other`.
  void swap(actor_addr& other) noexcept;

  /// @cond PRIVATE

  inline abstract_actor* operator->() const noexcept {
    return ptr_.get();
  }

  intptr_t compare(const actor_addr& other) const noexcept;

  intptr_t compare(const abstract_actor* other) const noexcept;

  inline intptr_t compare(const abstract_actor_ptr& other) const noexcept {
    return compare(other.get());
  }

  /// @endcond

private:
  inline abstract_actor* get() const noexcept {
    return ptr_.get();
  }

  explicit actor_addr(abstract_actor*);

  abstract_actor_ptr ptr_;
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
