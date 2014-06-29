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

#include "cppa/actor.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/detail/singletons.hpp"

#include "cppa/io/middleman.hpp"

namespace cppa {

namespace {
intptr_t compare_impl(const abstract_actor* lhs, const abstract_actor* rhs) {
    return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}
} // namespace <anonymous>

actor_addr::actor_addr(const invalid_actor_addr_t&) : m_ptr(nullptr) {}

actor_addr::actor_addr(abstract_actor* ptr) : m_ptr(ptr) {}

intptr_t actor_addr::compare(const actor_addr& other) const {
    return compare_impl(m_ptr.get(), other.m_ptr.get());
}

intptr_t actor_addr::compare(const abstract_actor* other) const {
    return compare_impl(m_ptr.get(), other);
}

actor_addr actor_addr::operator=(const invalid_actor_addr_t&) {
    m_ptr.reset();
    return *this;
}

actor_id actor_addr::id() const { return (m_ptr) ? m_ptr->id() : 0; }

node_id actor_addr::node() const {
    return m_ptr ? m_ptr->node() : detail::singletons::get_node_id();
}

bool actor_addr::is_remote() const {
    return m_ptr ? m_ptr->is_remote() : false;
}

std::set<std::string> actor_addr::interface() const {
    if (!m_ptr) return std::set<std::string>{};
    return m_ptr->interface();
}

} // namespace cppa
