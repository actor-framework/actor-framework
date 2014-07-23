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

#ifndef CAF_SEND_HPP
#define CAF_SEND_HPP

#include "caf/actor.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"

namespace caf {

/**
 * @brief Sends @p to a message under the identity of @p from.
 */
inline void send_tuple_as(const actor& from, const channel& to, message msg) {
  if (to)
    to->enqueue(from.address(), message_id::invalid, std::move(msg),
          nullptr);
}

/**
 * @brief Sends @p to a message under the identity of @p from.
 */
template <class... Ts>
void send_as(const actor& from, const channel& to, Ts&&... args) {
  send_tuple_as(from, to, make_message(std::forward<Ts>(args)...));
}

/**
 * @brief Anonymously sends @p to a message.
 */
inline void anon_send_tuple(const channel& to, message msg) {
  send_tuple_as(invalid_actor, to, std::move(msg));
}

/**
 * @brief Anonymously sends @p to a message.
 */
template <class... Ts>
inline void anon_send(const channel& to, Ts&&... args) {
  send_as(invalid_actor, to, std::forward<Ts>(args)...);
}

// implemented in local_actor.cpp
/**
 * @brief Anonymously sends @p whom an exit message.
 */
void anon_send_exit(const actor_addr& whom, uint32_t reason);

/**
 * @brief Anonymously sends @p whom an exit message.
 */
template <class ActorHandle>
inline void anon_send_exit(const ActorHandle& whom, uint32_t reason) {
  anon_send_exit(whom.address(), reason);
}

} // namespace caf

#endif // CAF_SEND_HPP
