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


#include "cppa/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace {

inline cppa::detail::actor_registry& registry() {
    return *(cppa::detail::singleton_manager::get_actor_registry());
}

} // namespace <anonymous>

namespace cppa {

actor::actor(std::uint32_t aid, const process_information_ptr& pptr)
    : m_id(aid), m_is_proxy(true), m_parent_process(pptr) {
    if (!pptr) {
        throw std::logic_error("parent == nullptr");
    }
}

bool actor::chained_enqueue(actor* sender, any_tuple msg) {
    enqueue(sender, std::move(msg));
    return false;
}

actor::actor(const process_information_ptr& pptr)
    : m_id(registry().next_id()), m_is_proxy(false), m_parent_process(pptr) {
    if (!pptr) {
        throw std::logic_error("parent == nullptr");
    }
}

void actor::link_to(intrusive_ptr<actor>&& other) {
    intrusive_ptr<actor> tmp(std::move(other));
    link_to(tmp);
}

void actor::unlink_from(intrusive_ptr<actor>&& other) {
    intrusive_ptr<actor> tmp(std::move(other));
    unlink_from(tmp);
}

bool actor::remove_backlink(intrusive_ptr<actor>&& to) {
    intrusive_ptr<actor> tmp(std::move(to));
    return remove_backlink(tmp);
}

bool actor::establish_backlink(intrusive_ptr<actor>&& to) {
    intrusive_ptr<actor> tmp(std::move(to));
    return establish_backlink(tmp);
}

} // namespace cppa
