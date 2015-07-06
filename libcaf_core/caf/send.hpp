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

#ifndef CAF_SEND_HPP
#define CAF_SEND_HPP

#include "caf/actor.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_addr.hpp"
#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"
#include "caf/typed_actor.hpp"
#include "caf/system_messages.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {

/// Sends `to` a message under the identity of `from` with priority `prio`.
template <class... Ts>
void send_as(const actor& from, message_priority prio,
             const channel& to, Ts&&... xs) {
  if (! to) {
    return;
  }
  message_id mid;
  to->enqueue(from.address(),
              prio == message_priority::high ? mid.with_high_priority() : mid,
              make_message(std::forward<Ts>(xs)...), nullptr);
}

/// Sends `to` a message under the identity of `from`.
template <class... Ts>
void send_as(const actor& from, const channel& to, Ts&&... xs) {
  send_as(from, message_priority::normal, to, std::forward<Ts>(xs)...);
}

/// Sends `to` a message under the identity of `from` with priority `prio`.
template <class... Sigs, class... Ts>
void send_as(const actor& from, message_priority prio,
             const typed_actor<Sigs...>& to, Ts&&... xs) {
  using token =
    detail::type_list<
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type
      >::type...>;
  token tk;
  check_typed_input(to, tk);
  send_as(from, prio, actor_cast<channel>(to), std::forward<Ts>(xs)...);
}

/// Sends `to` a message under the identity of `from`.
template <class... Sigs, class... Ts>
void send_as(const actor& from, const typed_actor<Sigs...>& to, Ts&&... xs) {
  using token =
    detail::type_list<
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type
      >::type...>;
  token tk;
  check_typed_input(to, tk);
  send_as(from, message_priority::normal,
          actor_cast<channel>(to), std::forward<Ts>(xs)...);
}

/// Anonymously sends `to` a message with priority `prio`.
template <class... Ts>
void anon_send(message_priority prio, const channel& to, Ts&&... xs) {
  send_as(invalid_actor, prio, to, std::forward<Ts>(xs)...);
}

/// Anonymously sends `to` a message.
template <class... Ts>
void anon_send(const channel& to, Ts&&... xs) {
  send_as(invalid_actor, message_priority::normal, to, std::forward<Ts>(xs)...);
}

/// Anonymously sends `to` a message with priority `prio`.
template <class... Sigs, class... Ts>
void anon_send(message_priority prio, const typed_actor<Sigs...>& to,
               Ts&&... xs) {
  using token =
    detail::type_list<
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type
      >::type...>;
  token tk;
  check_typed_input(to, tk);
  anon_send(prio, actor_cast<channel>(to), std::forward<Ts>(xs)...);
}

/// Anonymously sends `to` a message.
template <class... Sigs, class... Ts>
void anon_send(const typed_actor<Sigs...>& to, Ts&&... xs) {
  using token =
    detail::type_list<
      typename detail::implicit_conversions<
        typename std::decay<Ts>::type
      >::type...>;
  token tk;
  check_typed_input(to, tk);
  anon_send(message_priority::normal, actor_cast<channel>(to),
            std::forward<Ts>(xs)...);
}

/// Anonymously sends `to` an exit message.
inline void anon_send_exit(const actor_addr& to, uint32_t reason) {
  if (! to){
    return;
  }
  auto ptr = actor_cast<actor>(to);
  ptr->enqueue(invalid_actor_addr, message_id{}.with_high_priority(),
               make_message(exit_msg{invalid_actor_addr, reason}), nullptr);
}

/// Anonymously sends `to` an exit message.
template <class ActorHandle>
void anon_send_exit(const ActorHandle& to, uint32_t reason) {
  anon_send_exit(to.address(), reason);
}

} // namespace caf

#endif // CAF_SEND_HPP
