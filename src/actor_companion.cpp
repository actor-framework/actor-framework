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


#include "cppa/actor_companion.hpp"

namespace cppa {

void actor_companion::disconnect(std::uint32_t rsn) {
    enqueue_handler tmp;
    { // lifetime scope of guard
        std::lock_guard<lock_type> guard(m_lock);
        m_on_enqueue.swap(tmp);
    }
    cleanup(rsn);
}

void actor_companion::on_enqueue(enqueue_handler handler) {
    std::lock_guard<lock_type> guard(m_lock);
    m_on_enqueue = std::move(handler);
}

void actor_companion::enqueue(const message_header& hdr, any_tuple msg) {
    message_pointer ptr;
    ptr.reset(detail::memory::create<mailbox_element>(hdr, std::move(msg)));
    util::shared_lock_guard<lock_type> guard(m_lock);
    if (!m_on_enqueue) return;
    m_on_enqueue(std::move(ptr));
}

} // namespace cppa
