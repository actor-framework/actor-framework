/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_SEND_HPP
#define CAF_SEND_HPP

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_addr.hpp"
#include "caf/message_id.hpp"
#include "caf/typed_actor.hpp"
#include "caf/response_type.hpp"
#include "caf/system_messages.hpp"
#include "caf/is_message_sink.hpp"
#include "caf/message_priority.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {

/// Sends `to` a message under the identity of `from` with priority `prio`.
template <message_priority P = message_priority::normal,
          class Source = actor, class Dest = actor, class... Ts>
void send_as(const Source& src, const Dest& dest, Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  using token = detail::type_list<detail::strip_and_convert_t<Ts>...>;
  static_assert(!statically_typed<Source>() || statically_typed<Dest>(),
                "statically typed actors can only send() to other "
                "statically typed actors; use anon_send() or request() when "
                "communicating with dynamically typed actors");
  static_assert(response_type_unbox<
                  signatures_of_t<Dest>,
                  token
                >::valid,
                "receiver does not accept given message");
  // TODO: this only checks one way, we should check for loops
  static_assert(is_void_response<
                  response_type_unbox_t<
                    signatures_of_t<Dest>,
                    token>
                >::value
                ||  response_type_unbox<
                      signatures_of_t<Source>,
                      response_type_unbox_t<
                        signatures_of_t<Dest>,
                        token>
                    >::valid,
                "this actor does not accept the response message");
  if (dest)
    dest->eq_impl(message_id::make(P), actor_cast<strong_actor_ptr>(src),
                  nullptr, std::forward<Ts>(xs)...);
}

/// Anonymously sends `dest` a message.
template <message_priority P = message_priority::normal,
          class Dest = actor, class... Ts>
void anon_send(const Dest& dest, Ts&&... xs) {
  static_assert(sizeof...(Ts) > 0, "no message to send");
  using token = detail::type_list<detail::strip_and_convert_t<Ts>...>;
  static_assert(response_type_unbox<signatures_of_t<Dest>, token>::valid,
                "receiver does not accept given message");
  if (dest)
    dest->eq_impl(message_id::make(P), nullptr, nullptr,
                  std::forward<Ts>(xs)...);
}

/// Anonymously sends `dest` an exit message.
template <class Dest>
void anon_send_exit(const Dest& dest, exit_reason reason) {
  CAF_LOG_TRACE(CAF_ARG(dest) << CAF_ARG(reason));
  if (dest)
    dest->enqueue(nullptr, message_id::make(),
                  make_message(exit_msg{dest->address(), reason}), nullptr);
}

/// Anonymously sends `to` an exit message.
inline void anon_send_exit(const actor_addr& to, exit_reason reason) {
  auto ptr = actor_cast<strong_actor_ptr>(to);
  if (ptr)
    anon_send_exit(ptr, reason);
}

/// Anonymously sends `to` an exit message.
inline void anon_send_exit(const weak_actor_ptr& to, exit_reason reason) {
  auto ptr = actor_cast<strong_actor_ptr>(to);
  if (ptr)
    anon_send_exit(ptr, reason);
}

} // namespace caf

#endif // CAF_SEND_HPP
