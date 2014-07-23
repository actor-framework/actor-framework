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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
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
 * @brief Describes the behavior of an actor.
 */
class behavior {

  friend class message_handler;

 public:

  using continuation_fun = std::function<optional<message>(message&)>;

  /** @cond PRIVATE */

  using impl_ptr = intrusive_ptr<detail::behavior_impl>;

  inline auto as_behavior_impl() const -> const impl_ptr&;

  inline behavior(impl_ptr ptr);

  /** @endcond */

  behavior() = default;
  behavior(behavior&&) = default;
  behavior(const behavior&) = default;
  behavior& operator=(behavior&&) = default;
  behavior& operator=(const behavior&) = default;

  behavior(const message_handler& fun);

  template <class F>
  behavior(const timeout_definition<F>& arg);

  template <class F>
  behavior(const duration& d, F f);

  template <class T, class... Ts>
  behavior(const T& arg, Ts&&... args);

  /**
   * @brief Invokes the timeout callback.
   */
  inline void handle_timeout();

  /**
   * @brief Returns the duration after which receives using
   *    this behavior should time out.
   */
  inline const duration& timeout() const;

  /**
   * @brief Returns a value if @p arg was matched by one of the
   *    handler of this behavior, returns @p nothing otherwise.
   */
  template <class T>
  inline optional<message> operator()(T&& arg);

  /**
   * @brief Adds a continuation to this behavior that is executed
   *    whenever this behavior was successfully applied to
   *    a message.
   */
  behavior add_continuation(continuation_fun fun);

  inline operator bool() const { return static_cast<bool>(m_impl); }

 private:

  impl_ptr m_impl;

};

/**
 * @brief Creates a behavior from a match expression and a timeout definition.
 * @relates behavior
 */

template <class... Cs, typename F>
inline behavior operator, (const match_expr<Cs...>& lhs,
               const timeout_definition<F>& rhs) {
  return {lhs, rhs};
}

/******************************************************************************
 *       inline and template member function implementations      *
 ******************************************************************************/

template <class T, class... Ts>
behavior::behavior(const T& arg, Ts&&... args)
    : m_impl(detail::match_expr_concat(
        detail::lift_to_match_expr(arg),
        detail::lift_to_match_expr(std::forward<Ts>(args))...)) {}

template <class F>
behavior::behavior(const timeout_definition<F>& arg)
    : m_impl(detail::new_default_behavior(arg.timeout, arg.handler)) {}

template <class F>
behavior::behavior(const duration& d, F f)
    : m_impl(detail::new_default_behavior(d, f)) {}

inline behavior::behavior(impl_ptr ptr) : m_impl(std::move(ptr)) {}

inline void behavior::handle_timeout() { m_impl->handle_timeout(); }

inline const duration& behavior::timeout() const { return m_impl->timeout(); }

template <class T>
inline optional<message> behavior::operator()(T&& arg) {
  return (m_impl) ? m_impl->invoke(std::forward<T>(arg)) : none;
}

inline auto behavior::as_behavior_impl() const -> const impl_ptr& {
  return m_impl;
}

} // namespace caf

#endif // CAF_BEHAVIOR_HPP
