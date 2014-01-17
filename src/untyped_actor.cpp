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
#include "cppa/message_id.hpp"
#include "cppa/untyped_actor.hpp"

namespace cppa {

continue_helper& continue_helper::continue_with(behavior::continuation_fun fun) {
    auto ref_opt = m_self->bhvr_stack().sync_handler(m_mid);
    if (ref_opt) {
        behavior cpy = *ref_opt;
        *ref_opt = cpy.add_continuation(std::move(fun));
    }
    else { CPPA_LOG_ERROR("failed to add continuation"); }
    return *this;
}

void untyped_actor::forward_to(const actor& whom) {
    forward_message(whom, message_priority::normal);
}

untyped_actor::response_future untyped_actor::sync_send_tuple(const actor& dest,
                                                              any_tuple what) {
    auto nri = new_request_id();
    dest.enqueue({address(), dest, nri}, std::move(what));
    return {nri.response_id(), this};
}

untyped_actor::response_future
untyped_actor::timed_sync_send_tuple(const util::duration& rtime,
                                     const actor& dest,
                                     any_tuple what) {
    return {timed_sync_send_tuple_impl(message_priority::normal, dest, rtime,
                                       std::move(what)),
            this};
}

} // namespace cppa
