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

class behavior;

/**
 * @brief A partial function implementation
 *    for {@link caf::message messages}.
 */
class message_handler {

  friend class behavior;

 public:

  /** @cond PRIVATE */

  using impl_ptr = intrusive_ptr<detail::behavior_impl>;

  inline auto as_behavior_impl() const -> impl_ptr;

  message_handler(impl_ptr ptr);

  /** @endcond */

  message_handler() = default;
  message_handler(message_handler&&) = default;
  message_handler(const message_handler&) = default;
  message_handler& operator=(message_handler&&) = default;
  message_handler& operator=(const message_handler&) = default;

  template <class T, class... Ts>
  message_handler(const T& arg, Ts&&... args);

  /**
   * @brief Returns a value if @p arg was matched by one of the
   *    handler of this behavior, returns @p nothing otherwise.
   */
  template <class T>
  inline optional<message> operator()(T&& arg);

  /**
   * @brief Adds a fallback which is used where
   *    this partial function is not defined.
   */
  template <class... Ts>
  typename std::conditional<
    detail::disjunction<may_have_timeout<
      typename detail::rm_const_and_ref<Ts>::type>::value...>::value,
    behavior, message_handler>::type
  or_else(Ts&&... args) const;

 private:

  impl_ptr m_impl;

};

template <class... Cases>
message_handler operator, (const match_expr<Cases...>& mexpr,
               const message_handler& pfun) {
  return mexpr.as_behavior_impl()->or_else(pfun.as_behavior_impl());
}

template <class... Cases>
message_handler operator, (const message_handler& pfun,
               const match_expr<Cases...>& mexpr) {
  return pfun.as_behavior_impl()->or_else(mexpr.as_behavior_impl());
}

/******************************************************************************
 *       inline and template member function implementations      *
 ******************************************************************************/

template <class T, class... Ts>
message_handler::message_handler(const T& arg, Ts&&... args)
    : m_impl(detail::match_expr_concat(
        detail::lift_to_match_expr(arg),
        detail::lift_to_match_expr(std::forward<Ts>(args))...)) {}

template <class T>
inline optional<message> message_handler::operator()(T&& arg) {
  return (m_impl) ? m_impl->invoke(std::forward<T>(arg)) : none;
}

template <class... Ts>
typename std::conditional<
  detail::disjunction<may_have_timeout<
    typename detail::rm_const_and_ref<Ts>::type>::value...>::value,
  behavior, message_handler>::type
message_handler::or_else(Ts&&... args) const {
  // using a behavior is safe here, because we "cast"
  // it back to a message_handler when appropriate
  behavior tmp{std::forward<Ts>(args)...};
  return m_impl->or_else(tmp.as_behavior_impl());
}

inline auto message_handler::as_behavior_impl() const -> impl_ptr {
  return m_impl;
}

} // namespace caf

#endif // CAF_PARTIAL_FUNCTION_HPP
