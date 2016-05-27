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

#ifndef CAF_RESPONSE_PROMISE_HPP
#define CAF_RESPONSE_PROMISE_HPP

#include <vector>

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/message_id.hpp"

namespace caf {

/// A response promise can be used to deliver a uniquely identifiable
/// response message from the server (i.e. receiver of the request)
/// to the client (i.e. the sender of the request).
class response_promise {
public:
  /// Constructs an invalid response promise.
  response_promise();
  response_promise(local_actor* self, mailbox_element& src);

  response_promise(response_promise&&) = default;
  response_promise(const response_promise&) = default;
  response_promise& operator=(response_promise&&) = default;
  response_promise& operator=(const response_promise&) = default;

  /// Satisfies the promise by sending a non-error response message.
  template <class T, class... Ts>
  typename std::enable_if<
    (sizeof...(Ts) > 0) || ! std::is_convertible<T, error>::value,
    response_promise
  >::type
  deliver(T&&x, Ts&&... xs) {
    return deliver_impl(make_message(std::forward<T>(x), std::forward<Ts>(xs)...));
  }

  /// Satisfies the promise by sending an error response message.
  /// For non-requests, nothing is done.
  response_promise deliver(error x);

  /// Returns whether this response promise replies to an asynchronous message.
  bool async() const;

  /// Queries whether this promise is a valid promise that is not satisfied yet.
  inline bool pending() const {
    return ! stages_.empty() || source_;
  }

  /// @cond PRIVATE

  inline local_actor* self() {
    return self_;
  }

  /// @endcond

private:
  using forwarding_stack = std::vector<strong_actor_ptr>;

  response_promise deliver_impl(message response_message);

  local_actor* self_;
  strong_actor_ptr source_;
  forwarding_stack stages_;
  message_id id_;
};

} // namespace caf

#endif // CAF_RESPONSE_PROMISE_HPP
