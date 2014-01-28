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


#include "cppa/logging.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/singletons.hpp"
#include "cppa/blocking_actor.hpp"

#include "cppa/detail/actor_registry.hpp"

namespace cppa {

void blocking_actor::response_handle::await(behavior& bhvr) {
    m_self->dequeue_response(bhvr, m_mid);
}


void blocking_actor::await_all_other_actors_done() {
    get_actor_registry()->await_running_count_equal(1);
}

blocking_actor::response_handle
blocking_actor::sync_send_tuple(const actor& dest, any_tuple what) {
    auto nri = new_request_id();
    dest->enqueue({address(), dest, nri}, std::move(what));
    return {nri.response_id(), this};
}

blocking_actor::response_handle
blocking_actor::timed_sync_send_tuple(const util::duration& rtime,
                                              const actor& dest,
                                              any_tuple what) {
    return {timed_sync_send_tuple_impl(message_priority::normal, dest, rtime,
                                       std::move(what)),
            this};
}

void blocking_actor::quit(std::uint32_t reason) {
    planned_exit_reason(reason);
    throw actor_exited(reason);
}

} // namespace cppa
