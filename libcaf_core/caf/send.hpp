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

/**
 * Sends `to` a message under the identity of `from`.
 */
inline void send_tuple_as(const actor& from, const channel& to, message msg) {
  if (to) {
    to->enqueue(from.address(), invalid_message_id, std::move(msg), nullptr);
  }
}

inline void send_tuple_as(const actor& from, const channel& to,
                          message_priority prio, message msg) {
  if (to) {
    message_id id;
    if (prio == message_priority::high) {
      id = id.with_high_priority();
    }
    to->enqueue(from.address(), id, std::move(msg), nullptr);
  }
}

/**
 * Sends `to` a message under the identity of `from`.
 */
template <class... Ts>
void send_as(const actor& from, const channel& to, Ts&&... args) {
  send_tuple_as(from, to, make_message(std::forward<Ts>(args)...));
}

template <class... Ts>
void send_as(const actor& from, const channel& to, message_priority prio,
             Ts&&... args) {
  send_tuple_as(from, to, prio, make_message(std::forward<Ts>(args)...));
}

/**
 * Anonymously sends `to` a message.
 */
inline void anon_send_tuple(const channel& to, message msg) {
  send_tuple_as(invalid_actor, to, std::move(msg));
}

inline void anon_send_tuple(const channel& to, message_priority prio,
                            message msg) {
  send_tuple_as(invalid_actor, to, prio, std::move(msg));
}

/**
 * Anonymously sends `whom` a message.
 */
template <class... Ts>
void anon_send(const channel& to, Ts&&... args) {
  send_as(invalid_actor, to, std::forward<Ts>(args)...);
}

template <class... Ts>
void anon_send(const channel& to, message_priority prio, Ts&&... args) {
  send_as(invalid_actor, to, prio, std::forward<Ts>(args)...);
}

/**
 * Anonymously sends `whom` a message.
 */
template <class... Rs, class... Ts>
void anon_send(const typed_actor<Rs...>& whom, Ts&&... args) {
  check_typed_input(whom,
                    detail::type_list<typename detail::implicit_conversions<
                      typename std::decay<Ts>::type
                    >::type...>{});
  anon_send(actor_cast<channel>(whom), std::forward<Ts>(args)...);
}

template <class... Rs, class... Ts>
void anon_send(const typed_actor<Rs...>& whom, message_priority prio,
               Ts&&... args) {
  check_typed_input(whom,
                    detail::type_list<typename detail::implicit_conversions<
                      typename std::decay<Ts>::type
                    >::type...>{});
  anon_send(actor_cast<channel>(whom), prio, std::forward<Ts>(args)...);
}

/**
 * Anonymously sends `whom` an exit message.
 */
inline void anon_send_exit(const actor_addr& whom, uint32_t reason) {
  if (!whom){
    return;
  }
  auto ptr = actor_cast<actor>(whom);
  ptr->enqueue(invalid_actor_addr, message_id{}.with_high_priority(),
               make_message(exit_msg{invalid_actor_addr, reason}), nullptr);
}

/**
 * Anonymously sends `whom` an exit message.
 */
template <class ActorHandle>
void anon_send_exit(const ActorHandle& whom, uint32_t reason) {
  anon_send_exit(whom.address(), reason);
}

} // namespace caf

#endif // CAF_SEND_HPP
