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


#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"

#include "cppa/detail/raw_access.hpp"

namespace cppa {

channel::channel(const std::nullptr_t&) : m_ptr(nullptr) { }

channel::channel(const invalid_actor_t&) : m_ptr(nullptr) { }

channel::channel(const actor& other) : m_ptr(detail::raw_access::get(other)) { }

intptr_t channel::compare(const abstract_channel* lhs, const abstract_channel* rhs) {
    return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}

channel::channel(abstract_channel* ptr) : m_ptr(ptr) { }

channel::operator bool() const {
    return static_cast<bool>(m_ptr);
}

bool channel::operator!() const {
    return !m_ptr;
}

void channel::enqueue(const message_header& hdr, any_tuple msg) const {
    if (m_ptr) m_ptr->enqueue(hdr, std::move(msg));
}

intptr_t channel::compare(const channel& other) const {
    return compare(m_ptr.get(), other.m_ptr.get());
}

intptr_t channel::compare(const actor& other) const {
    return compare(m_ptr.get(), detail::raw_access::get(other));
}

intptr_t channel::compare(const abstract_channel* other) const {
    return compare(m_ptr.get(), other);
}

} // namespace cppa
