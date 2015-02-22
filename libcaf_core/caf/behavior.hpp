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

#ifndef CAF_BEHAVIOR_HPP
#define CAF_BEHAVIOR_HPP

#include <functional>
#include <type_traits>

#include "caf/none.hpp"

#include "caf/duration.hpp"
#include "caf/timeout_definition.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/behavior_impl.hpp"

namespace caf {

class message_handler;

/**
 * Describes the behavior of an actor, i.e., provides a message
 * handler and an optional timeout.
 */
class behavior {
 public:
  friend class message_handler;

  behavior() = default;
  behavior(behavior&&) = default;
  behavior(const behavior&) = default;
  behavior& operator=(behavior&&) = default;
  behavior& operator=(const behavior&) = default;

  /**
   * Creates a behavior from `fun` without timeout.
   */
  behavior(const message_handler& fun);

  /**
   * The list of arguments can contain match expressions, message handlers,
   * and up to one timeout (if set, the timeout has to be the last argument).
   */
  template <class V, class... Vs>
  behavior(V v, Vs... vs) {
    assign(std::move(v), std::move(vs)...);
  }

  /**
   * Creates a behavior from `tdef` without message handler.
   */
  template <class F>
  behavior(timeout_definition<F> tdef) : m_impl(detail::make_behavior(tdef)) {
    // nop
  }

  /**
   * Creates a behavior using `d` and `f` as timeout definition
   * without message handler.
   */
  //template <class F>
  //behavior(duration d, F f) : m_impl(detail::make_behavior(d, f)) {
    // nop
  //}

  /**
   * Assigns new handlers.
   */
  template <class... Vs>
  void assign(Vs... vs) {
    m_impl = detail::make_behavior(vs...);
  }

  /**
   * Equal to `*this = other`.
   */
  void assign(message_handler other);

  /**
   * Equal to `*this = other`.
   */
  void assign(behavior other);

  /**
   * Invokes the timeout callback if set.
   */
  inline void handle_timeout() {
    m_impl->handle_timeout();
  }

  /**
   * Returns the duration after which receive operations
   * using this behavior should time out.
   */
  inline const duration& timeout() const {
    return m_impl->timeout();
  }

  /**
   * Returns a value if `arg` was matched by one of the
   * handler of this behavior, returns `nothing` otherwise.
   */
  template <class T>
  optional<message> operator()(T&& arg) {
    return (m_impl) ? m_impl->invoke(std::forward<T>(arg)) : none;
  }

  /**
   * Checks whether this behavior is not empty.
   */
  inline operator bool() const {
    return static_cast<bool>(m_impl);
  }

  /** @cond PRIVATE */

  using impl_ptr = intrusive_ptr<detail::behavior_impl>;

  inline const impl_ptr& as_behavior_impl() const {
    return m_impl;
  }

  inline behavior(impl_ptr ptr) : m_impl(std::move(ptr)) {
    // nop
  }

  void assign(detail::behavior_impl*);

  /** @endcond */

 private:
  impl_ptr m_impl;
};

} // namespace caf

#endif // CAF_BEHAVIOR_HPP
