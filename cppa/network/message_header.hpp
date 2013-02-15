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
 * Free Software Foundation, either version 3 of the License                  *
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

#include <utility>

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_id.hpp"

namespace cppa { namespace network {

/**
 * @brief Encapsulates information about sender, receiver and (synchronous)
 *        message ID of a message. The message itself is usually an any_tuple.
 */
class message_header {

 public:

    actor_ptr    sender;
    actor_ptr    receiver;
    message_id_t id;

    message_header();

    message_header(const actor_ptr& sender,
                   const actor_ptr& receiver,
                   message_id_t id = message_id_t::invalid);

    inline void deliver(any_tuple msg) const {
        if (receiver) {
            if (id.valid()) {
                receiver->sync_enqueue(sender.get(), id, std::move(msg));
            }
            else {
                receiver->enqueue(sender.get(), std::move(msg));
            }
        }
    }

};

inline bool operator==(const message_header& lhs, const message_header& rhs) {
    return    lhs.sender == rhs.sender
           && lhs.receiver == rhs.receiver
           && lhs.id == rhs.id;
}

inline bool operator!=(const message_header& lhs, const message_header& rhs) {
    return !(lhs == rhs);
}

} } // namespace cppa::network

#endif // CPPA_MESSAGE_HEADER_HPP
