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


#include "cppa/cppa.hpp"
#include "cppa/receive.hpp"

namespace cppa {

void receive_loop(behavior& rules) {
    local_actor* sptr = self;
    for (;;) {
        sptr->dequeue(rules);
    }
}

void receive_loop(behavior&& rules) {
    behavior tmp(std::move(rules));
    receive_loop(tmp);
}

void receive_loop(partial_function& rules) {
    local_actor* sptr = self;
    for (;;) {
        sptr->dequeue(rules);
    }
}

void receive_loop(partial_function&& rules) {
    partial_function tmp(std::move(rules));
    receive_loop(tmp);
}

sync_recv_helper receive_response(const message_future& handle) {
    return {handle.id(), [](behavior& bhvr, message_id_t mf) {
        if (!self->awaits(mf)) {
            throw std::logic_error("response already received");
        }
        self->dequeue_response(bhvr, mf);
    }};
}

} // namespace cppa
