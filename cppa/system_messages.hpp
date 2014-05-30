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


#ifndef CPPA_SYSTEM_MESSAGES_HPP
#define CPPA_SYSTEM_MESSAGES_HPP

#include <cstdint>

#include "cppa/group.hpp"
#include "cppa/actor_addr.hpp"

#include "cppa/util/buffer.hpp"

#include "cppa/io/accept_handle.hpp"
#include "cppa/io/connection_handle.hpp"

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

/**
 * @brief Signalizes a newly accepted connection from a {@link broker}.
 */
struct new_connection_msg {
    /**
     * @brief The handle that accepted the new connection.
     */
    io::accept_handle source;
    /**
     * @brief The handle for the new connection.
     */
    io::connection_handle handle;
};

/**
 * @brief Signalizes newly arrived data for a {@link broker}.
 */
struct new_data_msg {
    /**
     * @brief Handle to the related connection
     */
    io::connection_handle handle;
    /**
     * @brief Buffer containing the received data.
     */
    util::buffer buf;
};

/**
 * @brief Signalizes that a {@link broker} connection has been closed.
 */
struct connection_closed_msg {
    /**
     * @brief Handle to the closed connection.
     */
    io::connection_handle handle;
};

/**
 * @brief Signalizes that a {@link broker} acceptor has been closed.
 */
struct acceptor_closed_msg {
    /**
     * @brief Handle to the closed connection.
     */
    io::accept_handle handle;
};

} // namespace cppa

#endif // CPPA_SYSTEM_MESSAGES_HPP
