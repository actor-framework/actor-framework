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


#include "cppa/scheduler.hpp"
#include "cppa/singletons.hpp"

namespace cppa {

message_future sync_send_tuple(actor_destination dest, any_tuple what) {
    if (!dest.receiver) throw std::invalid_argument("whom == nullptr");
    auto req = self->new_request_id();
    message_header hdr{self, std::move(dest.receiver), req, dest.priority};
    if (self->chaining_enabled()) {
        if (hdr.receiver->chained_enqueue(hdr, std::move(what))) {
            self->chained_actor(hdr.receiver.downcast<actor>());
        }
    }
    else hdr.deliver(std::move(what));
    return req.response_id();
}

void delayed_send_tuple(channel_destination dest,
                        const util::duration& rtime,
                        any_tuple data) {
    if (dest.receiver) {
        message_header hdr{self, std::move(dest.receiver), dest.priority};
        get_scheduler()->delayed_send(std::move(hdr), rtime, std::move(data));
    }
}

message_future timed_sync_send_tuple(actor_destination dest,
                                     const util::duration& rtime,
                                     any_tuple what) {
    auto mf = sync_send_tuple(std::move(dest), std::move(what));
    message_header hdr{self, self, mf.id()};
    auto tmp = make_any_tuple(atom("TIMEOUT"));
    get_scheduler()->delayed_send(std::move(hdr), rtime, std::move(tmp));
    return mf;
}

void delayed_reply_tuple(const util::duration& rtime,
                         message_id mid,
                         any_tuple data) {
    message_header hdr{self, self->last_sender(), mid};
    get_scheduler()->delayed_send(std::move(hdr), rtime, std::move(data));
}

void delayed_reply_tuple(const util::duration& rel_time, any_tuple data) {
    delayed_reply_tuple(rel_time, self->get_response_id(), std::move(data));
}

} // namespace cppa
