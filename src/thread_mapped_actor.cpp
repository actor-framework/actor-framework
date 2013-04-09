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


#include <chrono>
#include <memory>
#include <iostream>
#include <algorithm>

#include "cppa/self.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/thread_mapped_actor.hpp"

#include "cppa/detail/matches.hpp"

namespace cppa {

thread_mapped_actor::thread_mapped_actor() : super(std::function<void()>{}), m_initialized(true) { }

thread_mapped_actor::thread_mapped_actor(std::function<void()> fun)
: super(std::move(fun)), m_initialized(false) { }

/*
auto thread_mapped_actor::init_timeout(const util::duration& rel_time) -> timeout_type {
    auto result = std::chrono::high_resolution_clock::now();
    result += rel_time;
    return result;
}

void thread_mapped_actor::enqueue(const actor_ptr& sender, any_tuple msg) {
    auto ptr = new_mailbox_element(sender, std::move(msg));
    this->m_mailbox.push_back(ptr);
}

void thread_mapped_actor::sync_enqueue(const actor_ptr& sender,
                                       message_id id,
                                       any_tuple msg) {
    auto ptr = this->new_mailbox_element(sender, std::move(msg), id);
    if (!this->m_mailbox.push_back(ptr)) {
        detail::sync_request_bouncer f{this->exit_reason()};
        f(sender, id);
    }
}
*/

bool thread_mapped_actor::initialized() const {
    return m_initialized;
}

void thread_mapped_actor::cleanup(std::uint32_t reason) {
    detail::sync_request_bouncer f{reason};
    m_mailbox.close(f);
    super::cleanup(reason);
}


} // namespace cppa::detail
