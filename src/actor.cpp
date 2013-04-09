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


#include "cppa/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "cppa/actor.hpp"
#include "cppa/config.hpp"
#include "cppa/logging.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/singletons.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

namespace { typedef std::unique_lock<std::mutex> guard_type; }

// m_exit_reason is guaranteed to be set to 0, i.e., exit_reason::not_exited,
// by std::atomic<> constructor

actor::actor(actor_id aid)
: m_id(aid), m_is_proxy(true), m_exit_reason(exit_reason::not_exited) { }

actor::actor()
: m_id(get_actor_registry()->next_id()), m_is_proxy(false)
, m_exit_reason(exit_reason::not_exited) {
    CPPA_LOG_INFO("spawned new actor with ID " << id()
                  << ", class = " << CPPA_CLASS_NAME);
}

bool actor::chained_enqueue(const actor_ptr& sender, any_tuple msg) {
    enqueue(sender, std::move(msg));
    return false;
}

bool actor::chained_sync_enqueue(const actor_ptr& ptr, message_id id, any_tuple msg) {
    sync_enqueue(ptr, id, std::move(msg));
    return false;
}

bool actor::link_to_impl(const actor_ptr& other) {
    if (other && other != this) {
        guard_type guard(m_mtx);
        // send exit message if already exited
        if (exited()) {
            other->enqueue(this, make_any_tuple(atom("EXIT"), exit_reason()));
        }
        // add link if not already linked to other
        // (checked by establish_backlink)
        else if (other->establish_backlink(this)) {
            m_links.push_back(other);
            return true;
        }
    }
    return false;
}

bool actor::attach(attachable_ptr ptr) {
    if (ptr == nullptr) {
        guard_type guard(m_mtx);
        return m_exit_reason == exit_reason::not_exited;
    }
    std::uint32_t reason;
    { // lifetime scope of guard
        guard_type guard(m_mtx);
        reason = m_exit_reason;
        if (reason == exit_reason::not_exited) {
            m_attachables.push_back(std::move(ptr));
            return true;
        }
    }
    ptr->actor_exited(reason);
    return false;
}

void actor::detach(const attachable::token& what) {
    attachable_ptr ptr;
    { // lifetime scope of guard
        guard_type guard(m_mtx);
        auto end = m_attachables.end();
        auto i = std::find_if(
                    m_attachables.begin(), end,
                    [&](attachable_ptr& p) { return p->matches(what); });
        if (i != end) {
            ptr = std::move(*i);
            m_attachables.erase(i);
        }
    }
    // ptr will be destroyed here, without locked mutex
}

void actor::link_to(const actor_ptr& other) {
    static_cast<void>(link_to_impl(other));
}

void actor::unlink_from(const actor_ptr& other) {
    static_cast<void>(unlink_from_impl(other));
}

bool actor::remove_backlink(const actor_ptr& other) {
    if (other && other != this) {
        guard_type guard(m_mtx);
        auto i = std::find(m_links.begin(), m_links.end(), other);
        if (i != m_links.end()) {
            m_links.erase(i);
            return true;
        }
    }
    return false;
}

bool actor::establish_backlink(const actor_ptr& other) {
    std::uint32_t reason = exit_reason::not_exited;
    if (other && other != this) {
        guard_type guard(m_mtx);
        reason = m_exit_reason;
        if (reason == exit_reason::not_exited) {
            auto i = std::find(m_links.begin(), m_links.end(), other);
            if (i == m_links.end()) {
                m_links.push_back(other);
                return true;
            }
        }
    }
    // send exit message without lock
    if (reason != exit_reason::not_exited) {
        other->enqueue(this, make_any_tuple(atom("EXIT"), reason));
    }
    return false;
}

bool actor::unlink_from_impl(const actor_ptr& other) {
    guard_type guard(m_mtx);
    // remove_backlink returns true if this actor is linked to other
    if (other && !exited() && other->remove_backlink(this)) {
        auto i = std::find(m_links.begin(), m_links.end(), other);
        CPPA_REQUIRE(i != m_links.end());
        m_links.erase(i);
        return true;
    }
    return false;
}

void actor::cleanup(std::uint32_t reason) {
    CPPA_REQUIRE(reason != exit_reason::not_exited);
    // move everyhting out of the critical section before processing it
    decltype(m_links) mlinks;
    decltype(m_attachables) mattachables;
    { // lifetime scope of guard
        guard_type guard(m_mtx);
        if (m_exit_reason != exit_reason::not_exited) {
            // already exited
            return;
        }
        m_exit_reason = reason;
        mlinks = std::move(m_links);
        mattachables = std::move(m_attachables);
        // make sure lists are empty
        m_links.clear();
        m_attachables.clear();
    }
    // send exit messages
    for (actor_ptr& aptr : mlinks) {
        aptr->enqueue(this, make_any_tuple(atom("EXIT"), reason));
    }
    for (attachable_ptr& ptr : mattachables) {
        ptr->actor_exited(reason);
    }
}



} // namespace cppa
