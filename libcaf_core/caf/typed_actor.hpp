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

#ifndef CAF_TYPED_ACTOR_HPP
#define CAF_TYPED_ACTOR_HPP

#include "caf/intrusive_ptr.hpp"

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/replies_to.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/typed_response_promise.hpp"

namespace caf {

class actor_addr;
class local_actor;

struct invalid_actor_addr_t;

template <class... Sigs>
class typed_event_based_actor;

namespace io {
namespace experimental {

template <class... Sigs>
class typed_broker;

} // namespace experimental
} // namespace io

/// Identifies a statically typed actor.
/// @tparam Sigs Signature of this actor as `replies_to<...>::with<...>`
///              parameter pack.
template <class... Sigs>
class typed_actor : detail::comparable<typed_actor<Sigs...>>,
                    detail::comparable<typed_actor<Sigs...>, actor_addr>,
                    detail::comparable<typed_actor<Sigs...>,
                                       invalid_actor_addr_t> {
 public:
  friend class local_actor;

  // friend with all possible instantiations
  template <class...>
  friend class typed_actor;

  // allow conversion via actor_cast
  template <class T, typename U>
  friend T actor_cast(const U&);

  /// Creates a new `typed_actor` type by extending this one with `Es...`.
  template <class... Es>
  using extend = typed_actor<Sigs..., Es...>;

  /// Identifies the behavior type actors of this kind use
  /// for their behavior stack.
  using behavior_type = typed_behavior<Sigs...>;

  /// Identifies pointers to instances of this kind of actor.
  using pointer = typed_event_based_actor<Sigs...>*;

  /// Identifies the base class for this kind of actor.
  using base = typed_event_based_actor<Sigs...>;

  /// Identifies pointers to brokers implementing this interface.
  using broker_pointer = io::experimental::typed_broker<Sigs...>*;

  /// Identifies the base class of brokers implementing this interface.
  using broker_base = io::experimental::typed_broker<Sigs...>;

  typed_actor() = default;
  typed_actor(typed_actor&&) = default;
  typed_actor(const typed_actor&) = default;
  typed_actor& operator=(typed_actor&&) = default;
  typed_actor& operator=(const typed_actor&) = default;

  template <class... OtherSigs,
            class Enable = typename std::enable_if<
                             detail::tl_is_strict_subset<
                               detail::type_list<Sigs...>,
                               detail::type_list<OtherSigs...>
                             >::value
                           >::type>
  typed_actor(const typed_actor<OtherSigs...>& other) : ptr_(other.ptr_) {
    // nop
  }

  template <class... OtherSigs,
            class Enable = typename std::enable_if<
                             detail::tl_is_strict_subset<
                               detail::type_list<Sigs...>,
                               detail::type_list<OtherSigs...>
                             >::value
                           >::type>
  typed_actor& operator=(const typed_actor<OtherSigs...>& other) {
    ptr_ = other.ptr_;
    return *this;
  }

  template <class Impl,
            class Enable = typename std::enable_if<
                             detail::tl_is_strict_subset<
                               detail::type_list<Sigs...>,
                               typename Impl::signatures
                             >::value
                           >::type>
  typed_actor(intrusive_ptr<Impl> other) : ptr_(std::move(other)) {
    // nop
  }

  abstract_actor* operator->() const {
    return ptr_.get();
  }

  abstract_actor& operator*() const {
    return *ptr_.get();
  }

  /// Queries the address of the stored actor.
  actor_addr address() const {
    return ptr_ ? ptr_->address() : actor_addr{};
  }

  intptr_t compare(const actor_addr& rhs) const {
    return address().compare(rhs);
  }

  intptr_t compare(const typed_actor& other) const {
    return compare(other.address());
  }

  intptr_t compare(const invalid_actor_addr_t&) const {
    return ptr_ ? 1 : 0;
  }

  static std::set<std::string> message_types() {
    return {Sigs::static_type_name()...};
  }

  explicit operator bool() const {
    return static_cast<bool>(ptr_);
  }

  bool operator!() const {
    return !ptr_;
  }

private:
  abstract_actor* get() const {
    return ptr_.get();
  }

  typed_actor(abstract_actor* ptr) : ptr_(ptr) {
    // nop
  }

  abstract_actor_ptr ptr_;
};

} // namespace caf

#endif // CAF_TYPED_ACTOR_HPP
