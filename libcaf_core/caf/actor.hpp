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

#ifndef CAF_ACTOR_HPP
#define CAF_ACTOR_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "caf/intrusive_ptr.hpp"

#include "caf/fwd.hpp"
#include "caf/abstract_actor.hpp"

#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

class scoped_actor;

struct invalid_actor_t {
  constexpr invalid_actor_t() {}
};

/**
 * Identifies an invalid {@link actor}.
 * @relates actor
 */
constexpr invalid_actor_t invalid_actor = invalid_actor_t{};

template <class T>
struct is_convertible_to_actor {
  using type = typename std::remove_pointer<T>::type;
  static constexpr bool value = std::is_base_of<actor_proxy, type>::value
                                || std::is_base_of<local_actor, type>::value
                                || std::is_same<scoped_actor, type>::value;
};

/**
 * Identifies an untyped actor. Can be used with derived types
 * of `event_based_actor`, `blocking_actor`, `actor_proxy`.
 */
class actor : detail::comparable<actor>,
              detail::comparable<actor, actor_addr>,
              detail::comparable<actor, invalid_actor_t>,
              detail::comparable<actor, invalid_actor_addr_t> {

  friend class local_actor;

  template <class T, typename U>
  friend T actor_cast(const U&);

 public:

  actor() = default;

  actor(actor&&) = default;

  actor(const actor&) = default;

  template <class T>
  actor(intrusive_ptr<T> ptr,
      typename std::enable_if<is_convertible_to_actor<T>::value>::type* = 0)
      : m_ptr(std::move(ptr)) {}

  template <class T>
  actor(T* ptr,
      typename std::enable_if<is_convertible_to_actor<T>::value>::type* = 0)
      : m_ptr(ptr) {}

  actor(const invalid_actor_t&);

  actor& operator=(actor&&) = default;

  actor& operator=(const actor&) = default;

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

  actor& operator=(const invalid_actor_t&);

  inline operator bool() const {
    return static_cast<bool>(m_ptr);
  }

  inline bool operator!() const {
    return !m_ptr;
  }

  /**
   * Returns a handle that grants access to actor operations such as enqueue.
   */
  inline abstract_actor* operator->() const {
    return m_ptr.get();
  }

  inline abstract_actor& operator*() const {
    return *m_ptr;
  }

  intptr_t compare(const actor& other) const;

  intptr_t compare(const actor_addr&) const;

  inline intptr_t compare(const invalid_actor_t&) const {
    return m_ptr ? 1 : 0;
  }

  inline intptr_t compare(const invalid_actor_addr_t&) const {
    return compare(invalid_actor);
  }

  /**
   * Returns the address of the stored actor.
   */
  actor_addr address() const;

  actor_id id() const;

 private:

  void swap(actor& other);

  inline abstract_actor* get() const {
    return m_ptr.get();
  }

  actor(abstract_actor*);

  abstract_actor_ptr m_ptr;

};

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
