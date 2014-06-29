/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CPPA_SEND_HPP
#define CPPA_SEND_HPP

#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/message.hpp"
#include "cppa/actor_addr.hpp"

namespace cppa {

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
template<typename... Ts>
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
template<typename... Ts>
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
template<typename ActorHandle>
inline void anon_send_exit(const ActorHandle& whom, uint32_t reason) {
    anon_send_exit(whom.address(), reason);
}

} // namespace cppa

#endif // CPPA_SEND_HPP
