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


#include "cppa/detail/scheduled_actor_dummy.hpp"

namespace cppa { namespace detail {

scheduled_actor_dummy::scheduled_actor_dummy()
: scheduled_actor(actor_state::blocked, false) { }

void scheduled_actor_dummy::enqueue(const message_header&, any_tuple) { }
void scheduled_actor_dummy::quit(std::uint32_t) { }
void scheduled_actor_dummy::dequeue(behavior&) { }
void scheduled_actor_dummy::dequeue_response(behavior&, message_id) { }
void scheduled_actor_dummy::do_become(behavior&&, bool) { }
void scheduled_actor_dummy::become_waiting_for(behavior, message_id) { }
bool scheduled_actor_dummy::has_behavior() { return false; }

resume_result scheduled_actor_dummy::resume(util::fiber*, actor_ptr&) {
    return resume_result::actor_blocked;
}

scheduled_actor_type scheduled_actor_dummy::impl_type() {
    return event_based_impl;
}

} } // namespace cppa::detail
