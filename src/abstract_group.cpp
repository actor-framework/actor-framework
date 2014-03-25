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


#include "cppa/cppa.hpp"
#include "cppa/abstract_group.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/singletons.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

abstract_group::subscription::subscription(const channel& s,
                                  const intrusive_ptr<abstract_group>& g)
: m_subscriber(s), m_group(g) { }

abstract_group::subscription::~subscription() {
    if (valid()) m_group->unsubscribe(m_subscriber);
}

abstract_group::module::module(std::string name) : m_name(std::move(name)) { }

const std::string& abstract_group::module::name() {
    return m_name;
}

abstract_group::abstract_group(abstract_group::module_ptr mod, std::string id)
: m_module(mod), m_identifier(std::move(id)) { }

const std::string& abstract_group::identifier() const {
    return m_identifier;
}

abstract_group::module_ptr abstract_group::get_module() const {
    return m_module;
}

const std::string& abstract_group::module_name() const {
    return get_module()->name();
}

namespace {
struct group_nameserver : event_based_actor {
    behavior make_behavior() override {
        return (
            on(atom("GET_GROUP"), arg_match) >> [](const std::string& name) {
                return make_cow_tuple(atom("GROUP"), group::get("local", name));
            },
            on(atom("SHUTDOWN")) >> [=] {
                quit();
            }
        );
    }
};
} // namespace <anonymous>

void publish_local_groups(std::uint16_t port, const char* addr) {
    auto gn = spawn<group_nameserver, hidden>();
    try {
        publish(gn, port, addr);
    }
    catch (std::exception&) {
        gn->enqueue({invalid_actor_addr, nullptr},
                    make_any_tuple(atom("SHUTDOWN")),
                    nullptr);
        throw;
    }
}

abstract_group::module::~module() { }

abstract_group::~abstract_group() { }

} // namespace cppa
