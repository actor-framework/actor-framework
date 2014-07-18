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

#ifndef CAF_SYSTEM_MESSAGES_HPP
#define CAF_SYSTEM_MESSAGES_HPP

#include <vector>
#include <cstdint>
#include <type_traits>

#include "caf/group.hpp"
#include "caf/actor_addr.hpp"

#include "caf/detail/tbind.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {

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
    uint32_t reason;
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
    uint32_t reason;
};

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
    uint32_t reason;
};

template<typename T>
typename std::enable_if<
    detail::tl_exists<detail::type_list<exit_msg, down_msg, sync_exited_msg>,
                      detail::tbind<std::is_same, T>::template type>::value,
    bool>::type
operator==(const T& lhs, const T& rhs) {
    return lhs.source == rhs.source && lhs.reason == rhs.reason;
}

template<typename T>
typename std::enable_if<
    detail::tl_exists<detail::type_list<exit_msg, down_msg, sync_exited_msg>,
                      detail::tbind<std::is_same, T>::template type>::value,
    bool>::type
operator!=(const T& lhs, const T& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Sent to all members of a group when it goes offline.
 */
struct group_down_msg {
    /**
     * @brief The source of this message, i.e., the now unreachable group.
     */
    group source;
};

inline bool operator==(const group_down_msg& lhs, const group_down_msg& rhs) {
    return lhs.source == rhs.source;
}

inline bool operator!=(const group_down_msg& lhs, const group_down_msg& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Sent whenever a timeout occurs during a synchronous send.
 *
 * This system message does not have any fields, because the message ID
 * sent alongside this message identifies the matching request that timed out.
 */
struct sync_timeout_msg { };

/**
 * @relates exit_msg
 */
inline bool operator==(const sync_timeout_msg&, const sync_timeout_msg&) {
    return true;
}

/**
 * @relates exit_msg
 */
inline bool operator!=(const sync_timeout_msg&, const sync_timeout_msg&) {
    return false;
}

/**
 * @brief Signalizes a timeout event.
 * @note This message is handled implicitly by the runtime system.
 */
struct timeout_msg {
    /**
     * @brief Actor-specific timeout ID.
     */
    uint32_t timeout_id;
};

inline bool operator==(const timeout_msg& lhs, const timeout_msg& rhs) {
    return lhs.timeout_id == rhs.timeout_id;
}

inline bool operator!=(const timeout_msg& lhs, const timeout_msg& rhs) {
    return !(lhs == rhs);
}

} // namespace caf

#endif // CAF_SYSTEM_MESSAGES_HPP
