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


#include <utility>

#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/untyped_actor.hpp"
#include "cppa/blocking_untyped_actor.hpp"

namespace cppa {

actor::actor(const std::nullptr_t&) : m_ops(nullptr) { }

actor::actor(actor_proxy* ptr) : m_ops(ptr) { }

actor::actor(untyped_actor* ptr) : m_ops(ptr) { }

actor::actor(blocking_untyped_actor* ptr) : m_ops(ptr) { }

actor::actor(abstract_actor* ptr) : m_ops(ptr) { }

actor::actor(const invalid_actor_t&) : m_ops(nullptr) { }

void actor::handle::enqueue(const message_header& hdr, any_tuple msg) const {
    if (m_ptr) m_ptr->enqueue(hdr, std::move(msg));
}

intptr_t actor::compare(const actor& other) const {
    return channel::compare(m_ops.m_ptr.get(), other.m_ops.m_ptr.get());
}

} // namespace cppa
