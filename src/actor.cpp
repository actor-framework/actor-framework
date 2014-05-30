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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#include <utility>

#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/blocking_actor.hpp"
#include "cppa/event_based_actor.hpp"

namespace cppa {

actor::actor(const invalid_actor_t&) : m_ptr(nullptr) { }

actor::actor(abstract_actor* ptr) : m_ptr(ptr) { }

actor& actor::operator=(const invalid_actor_t&) {
    m_ptr.reset();
    return *this;
}

intptr_t actor::compare(const actor& other) const {
    return channel::compare(m_ptr.get(), other.m_ptr.get());
}

intptr_t actor::compare(const actor_addr& other) const {
    return m_ptr.compare(other.m_ptr);
}

void actor::swap(actor& other) {
    m_ptr.swap(other.m_ptr);
}

actor_addr actor::address() const {
    return m_ptr ? m_ptr->address() : actor_addr{};
}

} // namespace cppa
