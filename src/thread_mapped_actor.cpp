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
 * Copyright (C) 2011, 2012                                                   *
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

thread_mapped_actor::thread_mapped_actor() : m_initialized(true) { }

thread_mapped_actor::thread_mapped_actor(std::function<void()> fun)
: super(std::move(fun)), m_initialized(false) { }

void thread_mapped_actor::quit(std::uint32_t reason) {
    cleanup(reason);
    // actor_exited should not be catched, but if anyone does,
    // self must point to a newly created instance
    //self.set(nullptr);
    throw actor_exited(reason);
}

void thread_mapped_actor::enqueue(actor* sender, any_tuple msg) {
    m_mailbox.push_back(fetch_node(sender, std::move(msg)));
}

void thread_mapped_actor::sync_enqueue(actor* sender,
                                       message_id_t id,
                                       any_tuple msg ) {
    m_mailbox.push_back(fetch_node(sender, std::move(msg), id));
}

bool thread_mapped_actor::initialized() {
    return m_initialized;
}

} // namespace cppa::detail
