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


#include "cppa/cppa.hpp"
#include "cppa/group.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/singletons.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

intrusive_ptr<group> group::get(const std::string& arg0,
                                const std::string& arg1) {
    return get_group_manager()->get(arg0, arg1);
}

intrusive_ptr<group> group::anonymous() {
    return get_group_manager()->anonymous();
}

void group::add_module(group::unique_module_ptr ptr) {
    get_group_manager()->add_module(std::move(ptr));
}

group::module_ptr group::get_module(const std::string& module_name) {
    return get_group_manager()->get_module(module_name);
}

group::subscription::subscription(const channel_ptr& s,
                                  const intrusive_ptr<group>& g)
: m_subscriber(s), m_group(g) { }

group::subscription::~subscription() {
    if (valid()) m_group->unsubscribe(m_subscriber);
}

group::module::module(std::string name) : m_name(std::move(name)) { }

const std::string& group::module::name() {
    return m_name;
}

group::group(group::module_ptr mod, std::string id)
: m_module(mod), m_identifier(std::move(id)) { }

const std::string& group::identifier() const {
    return m_identifier;
}

group::module_ptr group::get_module() const {
    return m_module;
}

const std::string& group::module_name() const {
    return get_module()->name();
}

struct group_nameserver : event_based_actor {
    void init() {
        become (
            on(atom("GET_GROUP"), arg_match) >> [](const std::string& name) {
                reply(atom("GROUP"), group::get("local", name));
            },
            on(atom("SHUTDOWN")) >> [=] {
                quit();
            }
        );
    }
};

void publish_local_groups_at(std::uint16_t port, const char* addr) {
    auto gn = spawn<group_nameserver,hidden>();
    try {
        publish(gn, port, addr);
    }
    catch (std::exception&) {
        gn->enqueue(nullptr, make_any_tuple(atom("SHUTDOWN")));
        throw;
    }
}

} // namespace cppa
