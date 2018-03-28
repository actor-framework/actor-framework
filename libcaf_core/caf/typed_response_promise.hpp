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
class typed_response_promise {
public:
  /// Constructs an invalid response promise.
  typed_response_promise() = default;

  inline typed_response_promise(strong_actor_ptr self, mailbox_element& src)
      : promise_(std::move(self), src) {
    // nop
  }

  typed_response_promise(typed_response_promise&&) = default;
  typed_response_promise(const typed_response_promise&) = default;
  typed_response_promise& operator=(typed_response_promise&&) = default;
  typed_response_promise& operator=(const typed_response_promise&) = default;

  /// Implicitly convertible to untyped response promise.
  inline operator response_promise& () {
    return promise_;
  }

  /// Satisfies the promise by sending a non-error response message.
  template <class U, class... Us>
  typename std::enable_if<
    (sizeof...(Us) > 0) || !std::is_convertible<U, error>::value,
    typed_response_promise
  >::type
  deliver(U&& x, Us&&... xs) {
    static_assert(
      std::is_same<detail::type_list<Ts...>,
                   detail::type_list<typename std::decay<U>::type,
                                     typename std::decay<Us>::type...>>::value,
      "typed_response_promise: message type mismatched");
    promise_.deliver(std::forward<U>(x), std::forward<Us>(xs)...);
    return *this;
  }

  template <message_priority P = message_priority::normal,
           class Handle = actor, class... Us>
  typed_response_promise delegate(const Handle& dest, Us&&... xs) {
    promise_.template delegate<P>(dest, std::forward<Us>(xs)...);
    return *this;
  }

  /// Satisfies the promise by sending an error response message.
  /// For non-requests, nothing is done.
  inline typed_response_promise deliver(error x) {
    promise_.deliver(std::move(x));
    return *this;
  }

  /// Queries whether this promise is a valid promise that is not satisfied yet.
  inline bool pending() const {
    return promise_.pending();
  }

private:
  response_promise promise_;
};

} // namespace caf

