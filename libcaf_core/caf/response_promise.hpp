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

  response_promise(const actor_addr& from, const actor_addr& to,
                   const message_id& response_id);

  /// Queries whether this promise is still valid, i.e., no response
  /// was yet delivered to the client.
  inline explicit operator bool() const {
    // handle is valid if it has a receiver
    return static_cast<bool>(to_);
  }

  /// Sends `response_message` and invalidates this handle afterwards.
  void deliver(message response_message) const;

private:
  actor_addr from_;
  actor_addr to_;
  message_id id_;
};

} // namespace caf

#endif // CAF_RESPONSE_PROMISE_HPP
