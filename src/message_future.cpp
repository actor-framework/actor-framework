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


#include "cppa/self.hpp"
#include "cppa/logging.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/message_future.hpp"

namespace cppa {

continue_helper& continue_helper::continue_with(behavior::continuation_fun fun) {
    auto ref_opt = self->bhvr_stack().sync_handler(m_mid);
    if (ref_opt) {
        auto& ref = *ref_opt;
        // copy original behavior
        behavior cpy = ref;
        ref = cpy.add_continuation(std::move(fun));
    }
    else CPPA_LOG_ERROR(".continue_with: failed to add continuation");
    return *this;
}

continue_helper message_future::then(behavior bhvr) const {
    check_consistency();
    self->become_waiting_for(std::move(bhvr), m_mid);
    return {m_mid};
}

void message_future::await(behavior bhvr) const {
    check_consistency();
    self->dequeue_response(bhvr, m_mid);
}

match_hint message_future::handle_sync_timeout() {
    self->handle_sync_timeout();
    return match_hint::skip;
}

void message_future::check_consistency() const {
    if (!m_mid.valid() || !m_mid.is_response()) {
        throw std::logic_error("handle does not point to a response");
    }
    else if (!self->awaits(m_mid)) {
        throw std::logic_error("response already received");
    }
}

} // namespace cppa
