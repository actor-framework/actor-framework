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


#ifndef CPPA_MESSAGE_HEADER_HPP
#define CPPA_MESSAGE_HEADER_HPP

#include "cppa/self.hpp"
#include "cppa/actor.hpp"
#include "cppa/message_id.hpp"
#include "cppa/message_priority.hpp"

namespace cppa {

class any_tuple;

/**
 * @brief Encapsulates information about sender, receiver and (synchronous)
 *        message ID of a message. The message itself is usually an any_tuple.
 */
class message_header {

 public:

    actor_ptr   sender;
    channel_ptr receiver;
    message_id id;
    message_priority priority;

    /**
     * @brief An invalid message header without receiver or sender;
     **/
    message_header() = default;

    /**
     * @brief Creates a message header with <tt>receiver = dest</tt>
     *        and <tt>sender = self</tt>.
     **/
    template<typename T>
    message_header(intrusive_ptr<T> dest)
    : sender(self), receiver(dest), priority(message_priority::normal) {
        static_assert(std::is_convertible<T*, channel*>::value,
                      "illegal receiver");
    }

    template<typename T>
    message_header(T* dest)
    : sender(self), receiver(dest), priority(message_priority::normal) {
        static_assert(std::is_convertible<T*, channel*>::value,
                      "illegal receiver");
    }

    message_header(const std::nullptr_t&);

    /**
     * @brief Creates a message header with <tt>receiver = self</tt>
     *        and <tt>sender = self</tt>.
     **/
    message_header(const self_type&);

    /**
     * @brief Creates a message header with <tt>receiver = dest</tt>
     *        and <tt>sender = self</tt>.
     */
    message_header(channel_ptr dest,
                   message_id mid,
                   message_priority prio = message_priority::normal);

    /**
     * @brief Creates a message header with <tt>receiver = dest</tt> and
     *        <tt>sender = self</tt>.
     */
    message_header(channel_ptr dest, message_priority prio);

    /**
     * @brief Creates a message header with <tt>receiver = dest</tt> and
     *        <tt>sender = source</tt>.
     */
    message_header(actor_ptr source,
                   channel_ptr dest,
                   message_id mid = message_id::invalid,
                   message_priority prio = message_priority::normal);

    /**
     * @brief Creates a message header with <tt>receiver = dest</tt> and
     *        <tt>sender = self</tt>.
     */
    message_header(actor_ptr source, channel_ptr dest, message_priority prio);

    void deliver(any_tuple msg) const;

};

bool operator==(const message_header& lhs, const message_header& rhs);

bool operator!=(const message_header& lhs, const message_header& rhs);

} // namespace cppa

#endif // CPPA_MESSAGE_HEADER_HPP
