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

#ifndef CAF_PARTIAL_FUNCTION_HPP
#define CAF_PARTIAL_FUNCTION_HPP

#include <list>
#include <vector>
#include <memory>
#include <utility>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/on.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/behavior.hpp"
#include "caf/ref_counted.hpp"
#include "caf/may_have_timeout.hpp"
#include "caf/timeout_definition.hpp"

#include "caf/detail/behavior_impl.hpp"

namespace caf {

/**
 * A partial function implementation used to process a `message`.
 */
class message_handler {
 public:
  message_handler() = default;
  message_handler(message_handler&&) = default;
  message_handler(const message_handler&) = default;
  message_handler& operator=(message_handler&&) = default;
  message_handler& operator=(const message_handler&) = default;

  /**
   * A pointer to the underlying implementation.
   */
  using impl_ptr = intrusive_ptr<detail::behavior_impl>;

  /**
   * Returns a pointer to the implementation.
   */
  inline const impl_ptr& as_behavior_impl() const {
    return m_impl;
  }

  /**
   * Creates a message handler from @p ptr.
   */
  message_handler(impl_ptr ptr);

  /**
   * Create a message handler a list of match expressions,
   * functors, or other message handlers.
   */
  template <class T, class... Ts>
  message_handler(const T& arg, Ts&&... args)
      : m_impl(detail::match_expr_concat(
                 detail::lift_to_match_expr(arg),
                 detail::lift_to_match_expr(std::forward<Ts>(args))...)) {
    // nop
  }

  /**
   * Runs this handler and returns its (optional) result.
   */
  template <class T>
  optional<message> operator()(T&& arg) {
    return (m_impl) ? m_impl->invoke(std::forward<T>(arg)) : none;
  }

  /**
   * Returns a new handler that concatenates this handler
   * with a new handler from `args...`.
   */
  template <class... Ts>
  typename std::conditional<
    detail::disjunction<may_have_timeout<
      typename std::decay<Ts>::type>::value...>::value,
    behavior,
    message_handler
  >::type
  or_else(Ts&&... args) const {
    // using a behavior is safe here, because we "cast"
    // it back to a message_handler when appropriate
    behavior tmp{std::forward<Ts>(args)...};
    return m_impl->or_else(tmp.as_behavior_impl());
  }

 private:
  impl_ptr m_impl;
};

/**
 * Enables concatenation of message handlers by using a list
 * of commata separated handlers.
 * @relates message_handler
 */
template <class... Cases>
message_handler operator,(const match_expr<Cases...>& mexpr,
                          const message_handler& pfun) {
  return mexpr.as_behavior_impl()->or_else(pfun.as_behavior_impl());
}

/**
 * Enables concatenation of message handlers by using a list
 * of commata separated handlers.
 * @relates message_handler
 */
template <class... Cases>
message_handler operator,(const message_handler& pfun,
                          const match_expr<Cases...>& mexpr) {
  return pfun.as_behavior_impl()->or_else(mexpr.as_behavior_impl());
}

} // namespace caf

#endif // CAF_PARTIAL_FUNCTION_HPP
