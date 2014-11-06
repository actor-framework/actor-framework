/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

namespace caf {

class actor_addr;
class local_actor;

struct invalid_actor_addr_t;

template <class... Rs>
class typed_event_based_actor;

/**
 * Identifies a strongly typed actor.
 * @tparam Rs Interface as `replies_to<...>::with<...>` parameter pack.
 */
template <class... Rs>
class typed_actor
  : detail::comparable<typed_actor<Rs...>>,
    detail::comparable<typed_actor<Rs...>, actor_addr>,
    detail::comparable<typed_actor<Rs...>, invalid_actor_addr_t> {

  friend class local_actor;

  // friend with all possible instantiations
  template <class...>
  friend class typed_actor;

  // allow conversion via actor_cast
  template <class T, typename U>
  friend T actor_cast(const U&);

 public:

  template <class... Es>
  struct extend {
    using type = typed_actor<Rs..., Es...>;
  };

  /**
   * Identifies the behavior type actors of this kind use
   * for their behavior stack.
   */
  using behavior_type = typed_behavior<Rs...>;

  /**
   * Identifies pointers to instances of this kind of actor.
   */
  using pointer = typed_event_based_actor<Rs...>*;

  /**
   * Identifies the base class for this kind of actor.
   */
  using base = typed_event_based_actor<Rs...>;

  typed_actor() = default;
  typed_actor(typed_actor&&) = default;
  typed_actor(const typed_actor&) = default;
  typed_actor& operator=(typed_actor&&) = default;
  typed_actor& operator=(const typed_actor&) = default;

  template <class... OtherRs>
  typed_actor(const typed_actor<OtherRs...>& other) {
    set(std::move(other));
  }

  template <class... OtherRs>
  typed_actor& operator=(const typed_actor<OtherRs...>& other) {
    set(std::move(other));
    return *this;
  }

  template <class Impl>
  typed_actor(intrusive_ptr<Impl> other) {
    set(other);
  }

  abstract_actor* operator->() const {
    return m_ptr.get();
  }

  abstract_actor& operator*() const {
    return *m_ptr.get();
  }

  /**
   * Queries the address of the stored actor.
   */
  actor_addr address() const {
    return m_ptr ? m_ptr->address() : actor_addr{};
  }

  inline intptr_t compare(const actor_addr& rhs) const {
    return address().compare(rhs);
  }

  inline intptr_t compare(const typed_actor& other) const {
    return compare(other.address());
  }

  inline intptr_t compare(const invalid_actor_addr_t&) const {
    return m_ptr ? 1 : 0;
  }

  static std::set<std::string> message_types() {
    return {detail::to_uniform_name<Rs>()...};
  }

  explicit operator bool() const { return static_cast<bool>(m_ptr); }

  inline bool operator!() const { return !m_ptr; }

 private:

  inline abstract_actor* get() const { return m_ptr.get(); }

  typed_actor(abstract_actor* ptr) : m_ptr(ptr) {}

  template <class ListA, class ListB>
  inline void check_signatures() {
    static_assert(detail::tl_is_strict_subset<ListA, ListB>::value,
            "'this' must be a strict subset of 'other'");
  }

  template <class... OtherRs>
  inline void set(const typed_actor<OtherRs...>& other) {
    check_signatures<detail::type_list<Rs...>, detail::type_list<OtherRs...>>();
    m_ptr = other.m_ptr;
  }

  template <class Impl>
  inline void set(intrusive_ptr<Impl>& other) {
    check_signatures<detail::type_list<Rs...>, typename Impl::signatures>();
    m_ptr = std::move(other);
  }

  abstract_actor_ptr m_ptr;

};

} // namespace caf

#endif // CAF_TYPED_ACTOR_HPP
