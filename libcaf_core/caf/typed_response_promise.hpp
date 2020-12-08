/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/response_promise.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

/// A response promise can be used to deliver a uniquely identifiable
/// response message from the server (i.e. receiver of the request)
/// to the client (i.e. the sender of the request).
template <class... Ts>
class typed_response_promise : public response_promise {
public:
  using super = response_promise;

  /// Constructs an invalid response promise.
  typed_response_promise() = default;

  typed_response_promise(strong_actor_ptr self, mailbox_element& src)
    : super(std::move(self), src) {
    // nop
  }

  typed_response_promise(typed_response_promise&&) = default;
  typed_response_promise(const typed_response_promise&) = default;
  typed_response_promise& operator=(typed_response_promise&&) = default;
  typed_response_promise& operator=(const typed_response_promise&) = default;

  ~typed_response_promise() override = default;

  /// Satisfies the promise by sending a non-error response message.
  template <class U, class... Us>
  typename std::enable_if<(sizeof...(Us) > 0)
                            || !std::is_convertible<U, error>::value,
                          typed_response_promise>::type
  deliver(U&& x, Us&&... xs) {
    static_assert(
      std::is_same<detail::type_list<Ts...>,
                   detail::type_list<typename std::decay<U>::type,
                                     typename std::decay<Us>::type...>>::value,
      "typed_response_promise: message type mismatched");
    super::deliver(std::forward<U>(x), std::forward<Us>(xs)...);
    return *this;
  }

  /// Satisfies the promise by sending an error or non-error response message.
  template <class T>
  void deliver(expected<T> x) {
    if (x)
      return deliver(std::move(*x));
    return deliver(std::move(x.error()));
  }

  /// Satisfies the promise by sending an error response message.
  /// For non-requests, nothing is done.
  typed_response_promise deliver(error x) {
    super::deliver(std::move(x));
    return *this;
  }

  /// Satisfies the promise by sending an empty message if this promise has a
  /// valid message ID, i.e., `async() == false`.
  void deliver(unit_t x) {
    super::deliver(x);
  }

  /// Satisfies the promise by delegating to another actor.
  template <message_priority P = message_priority::normal, class Handle = actor,
            class... Us>
  typed_response_promise delegate(const Handle& dest, Us&&... xs) {
    super::template delegate<P>(dest, std::forward<Us>(xs)...);
    return *this;
  }

private:
  using super::delegate;
  using super::deliver;
};

} // namespace caf
