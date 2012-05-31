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


#include "cppa/group.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

intrusive_ptr<group> group::get(const std::string& arg0,
                                const std::string& arg1) {
    return detail::singleton_manager::get_group_manager()->get(arg0, arg1);
}

intrusive_ptr<group> group::anonymous() {
    return detail::singleton_manager::get_group_manager()->anonymous();
}

void group::add_module(group::module* ptr) {
    detail::singleton_manager::get_group_manager()->add_module(ptr);
}

group::unsubscriber::unsubscriber(const channel_ptr& s,
                                  const intrusive_ptr<group>& g)
    : m_self(s), m_group(g) {
}

group::unsubscriber::~unsubscriber() {
    if (m_group && m_self) m_group->unsubscribe(m_self);
}

void group::unsubscriber::actor_exited(std::uint32_t) {
    // unsubscription is done in destructor
}

bool group::unsubscriber::matches(const attachable::token& what) {
    if (what.subtype == typeid(group::unsubscriber)) {
        return m_group == reinterpret_cast<const group*>(what.ptr);
    }
    return false;
}

group::module::module(std::string name) : m_name(std::move(name)) { }

const std::string& group::module::name() {
    return m_name;
}

group::group(std::string&& id, std::string&& mod_name)
    : m_identifier(std::move(id)), m_module_name(std::move(mod_name)) {
}

group::group(const std::string& id, const std::string& mod_name)
    : m_identifier(id), m_module_name(mod_name) {
}

const std::string& group::identifier() const {
    return m_identifier;
}

const std::string& group::module_name() const {
    return m_module_name;
}

} // namespace cppa
