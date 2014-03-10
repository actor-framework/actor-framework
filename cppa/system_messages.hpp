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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_SYSTEM_MESSAGES_HPP
#define CPPA_SYSTEM_MESSAGES_HPP

#include <cstdint>

#include "cppa/group.hpp"
#include "cppa/actor_addr.hpp"

namespace cppa {

/**
 * @brief Sent to all links when an actor is terminated.
 * @note This message can be handled manually by calling
 *       {@link local_actor::trap_exit(true) local_actor::trap_exit(bool)}
 *       and is otherwise handled implicitly by the runtime system.
 */
struct exit_msg {
    /**
     * @brief The source of this message, i.e., the terminated actor.
     */
    actor_addr source;
    /**
     * @brief The exit reason of the terminated actor.
     */
    std::uint32_t reason;
};

/**
 * @brief Sent to all actors monitoring an actor when it is terminated.
 */
struct down_msg {
    /**
     * @brief The source of this message, i.e., the terminated actor.
     */
    actor_addr source;
    /**
     * @brief The exit reason of the terminated actor.
     */
    std::uint32_t reason;
};

/**
 * @brief Sent to all members of a group when it goes offline.
 */
struct group_down_msg {
    /**
     * @brief The source of this message, i.e., the now unreachable group.
     */
    group source;
};

/**
 * @brief Sent whenever a timeout occurs during a synchronous send.
 *
 * This system message does not have any fields, because the message ID
 * sent alongside this message identifies the matching request that timed out.
 */
struct sync_timeout_msg { };

/**
 * @brief Sent whenever a terminated actor receives a synchronous request.
 */
struct sync_exited_msg {
    /**
     * @brief The source of this message, i.e., the terminated actor.
     */
    actor_addr source;
    /**
     * @brief The exit reason of the terminated actor.
     */
    std::uint32_t reason;
};

/**
 * @brief Signalizes a timeout event.
 * @note This message is handled implicitly by the runtime system.
 */
struct timeout_msg {
    /**
     * @brief Actor-specific timeout ID.
     */
    std::uint32_t timeout_id;
};

} // namespace cppa

#endif // CPPA_SYSTEM_MESSAGES_HPP
