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
  response_promise() = default;
  response_promise(response_promise&&) = default;
  response_promise(const response_promise&) = default;
  response_promise& operator=(response_promise&&) = default;
  response_promise& operator=(const response_promise&) = default;

  using forwarding_stack = std::vector<actor_addr>;

  //response_promise(local_actor* self, actor_addr source,
  //                 forwarding_stack stages,
  //                 message_id response_id);

  response_promise(local_actor* self, mailbox_element& src);

  /// Queries whether this promise is still valid, i.e., no response
  /// was yet delivered to the client.
  inline bool valid() const {
    // handle is valid if it has a receiver or a next stage
    return source_ || ! stages_.empty();
  }

  inline explicit operator bool() const {
    return valid();
  }

  /// Sends the response_message and invalidate this promise.
  template <class T, class... Ts>
  typename std::enable_if<
    ! std::is_convertible<T, error>::value
  >::type
  deliver(T&&x, Ts&&... xs) const {
    deliver_impl(make_message(std::forward<T>(x), std::forward<Ts>(xs)...));
  }

  /// Sends an error as response unless the sender used asynchronous messaging
  /// and invalidate this promise.
  void deliver(error x) const;

private:
  void deliver_impl(message response_message) const;
  local_actor* self_;
  mutable actor_addr source_;
  mutable forwarding_stack stages_;
  message_id id_;
};

} // namespace caf

#endif // CAF_RESPONSE_PROMISE_HPP
