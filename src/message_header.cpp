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


#include "cppa/any_tuple.hpp"
#include "cppa/message_header.hpp"

namespace cppa {

message_header::message_header(actor_addr source,
                               channel dest,
                               message_id mid,
                               message_priority prio)
: sender(source), receiver(dest), id(mid), priority(prio) { }

message_header::message_header(actor_addr source,
                               channel dest,
                               message_priority prio)
: sender(source), receiver(dest), priority(prio) { }

bool operator==(const message_header& lhs, const message_header& rhs) {
    return    lhs.sender == rhs.sender
           && lhs.receiver == rhs.receiver
           && lhs.id == rhs.id
           && lhs.priority == rhs.priority;
}

bool operator!=(const message_header& lhs, const message_header& rhs) {
    return !(lhs == rhs);
}

void message_header::deliver(any_tuple msg) const {
    receiver.enqueue(*this, std::move(msg));
}

} // namespace cppa::network
