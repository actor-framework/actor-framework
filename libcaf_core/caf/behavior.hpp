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

#ifndef CAF_BEHAVIOR_HPP
#define CAF_BEHAVIOR_HPP

#include <functional>
#include <type_traits>

#include "caf/none.hpp"

#include "caf/duration.hpp"
#include "caf/match_expr.hpp"
#include "caf/timeout_definition.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

class message_handler;

/**
 * Describes the behavior of an actor, i.e., provides a message
 * handler and an optional timeout.
 */
class behavior {
 public:
  friend class message_handler;

  /**
   * The type of continuations that can be used to extend an existing behavior.
   */
  using continuation_fun = std::function<optional<message>(message&)>;

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
  template <class T, class... Ts>
  behavior(const T& arg, Ts&&... args)
      : m_impl(detail::match_expr_concat(
                 detail::lift_to_match_expr(arg),
                 detail::lift_to_match_expr(std::forward<Ts>(args))...)) {
    // nop
  }

  /**
   * Creates a behavior from `tdef` without message handler.
   */
  template <class F>
  behavior(const timeout_definition<F>& tdef)
      : m_impl(detail::new_default_behavior(tdef.timeout, tdef.handler)) {
    // nop
  }

  /**
   * Creates a behavior using `d` and `f` as timeout definition
   * without message handler.
   */
  template <class F>
  behavior(const duration& d, F f)
      : m_impl(detail::new_default_behavior(d, f)) {
    // nop
  }

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
   * Adds a continuation to this behavior that is executed
   * whenever this behavior was successfully applied to a message.
   */
  behavior add_continuation(continuation_fun fun);

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

  /** @endcond */

 private:
  impl_ptr m_impl;
};

/**
 * Creates a behavior from a match expression and a timeout definition.
 * @relates behavior
 */
template <class... Cs, typename F>
behavior operator,(const match_expr<Cs...>& lhs,
                   const timeout_definition<F>& rhs) {
  return {lhs, rhs};
}

} // namespace caf

#endif // CAF_BEHAVIOR_HPP
