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

#include "cppa/atom.hpp"
#include "cppa/config.hpp"
#include "cppa/logging.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/singletons.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/message_header.hpp"
#include "cppa/system_messages.hpp"

#include "cppa/io/middleman.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

#include "cppa/detail/raw_access.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

namespace { typedef std::unique_lock<std::mutex> guard_type; }

// m_exit_reason is guaranteed to be set to 0, i.e., exit_reason::not_exited,
// by std::atomic<> constructor

abstract_actor::abstract_actor(actor_id aid)
        : m_id(aid), m_is_proxy(true)
        , m_exit_reason(exit_reason::not_exited), m_host(nullptr) { }

abstract_actor::abstract_actor()
        : m_id(get_actor_registry()->next_id()), m_is_proxy(false)
        , m_exit_reason(exit_reason::not_exited) {
    m_node = get_middleman()->node();
}

bool abstract_actor::link_to_impl(const actor_addr& other) {
    if (other && other != this) {
        guard_type guard{m_mtx};
        auto ptr = detail::raw_access::get(other);
        // send exit message if already exited
        if (exited()) {
            ptr->enqueue({address(), ptr},
                         make_any_tuple(exit_msg{address(), exit_reason()}),
                         m_host);
        }
        // add link if not already linked to other
        // (checked by establish_backlink)
        else if (ptr->establish_backlink(address())) {
            m_links.push_back(ptr);
            return true;
        }
    }
    return false;
}

bool abstract_actor::attach(attachable_ptr ptr) {
    if (ptr == nullptr) {
        guard_type guard{m_mtx};
        return m_exit_reason == exit_reason::not_exited;
    }
    std::uint32_t reason;
    { // lifetime scope of guard
        guard_type guard{m_mtx};
        reason = m_exit_reason;
        if (reason == exit_reason::not_exited) {
            m_attachables.push_back(std::move(ptr));
            return true;
        }
    }
    ptr->actor_exited(reason);
    return false;
}

void abstract_actor::detach(const attachable::token& what) {
    attachable_ptr ptr;
    { // lifetime scope of guard
        guard_type guard{m_mtx};
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

void abstract_actor::link_to(const actor_addr& other) {
    static_cast<void>(link_to_impl(other));
}

void abstract_actor::unlink_from(const actor_addr& other) {
    static_cast<void>(unlink_from_impl(other));
}

bool abstract_actor::remove_backlink(const actor_addr& other) {
    if (other && other != this) {
        guard_type guard{m_mtx};
        auto i = std::find(m_links.begin(), m_links.end(), other);
        if (i != m_links.end()) {
            m_links.erase(i);
            return true;
        }
    }
    return false;
}

bool abstract_actor::establish_backlink(const actor_addr& other) {
    std::uint32_t reason = exit_reason::not_exited;
    if (other && other != this) {
        guard_type guard{m_mtx};
        reason = m_exit_reason;
        if (reason == exit_reason::not_exited) {
            auto i = std::find(m_links.begin(), m_links.end(), other);
            if (i == m_links.end()) {
                m_links.push_back(detail::raw_access::get(other));
                return true;
            }
        }
    }
    // send exit message without lock
    if (reason != exit_reason::not_exited) {
        auto ptr = detail::raw_access::unsafe_cast(other);
        ptr->enqueue({address(), ptr},
                     make_any_tuple(exit_msg{address(), exit_reason()}),
                     m_host);
    }
    return false;
}

bool abstract_actor::unlink_from_impl(const actor_addr& other) {
    if (!other) return false;
    guard_type guard{m_mtx};
    // remove_backlink returns true if this actor is linked to other
    auto ptr = detail::raw_access::get(other);
    if (!exited() && ptr->remove_backlink(address())) {
        auto i = std::find(m_links.begin(), m_links.end(), ptr);
        CPPA_REQUIRE(i != m_links.end());
        m_links.erase(i);
        return true;
    }
    return false;
}

actor_addr abstract_actor::address() const {
    return actor_addr{const_cast<abstract_actor*>(this)};
}

void abstract_actor::cleanup(std::uint32_t reason) {
    // log as 'actor'
    CPPA_LOGM_TRACE("cppa::actor", CPPA_ARG(m_id) << ", " << CPPA_ARG(reason)
                    << ", " << CPPA_ARG(m_is_proxy));
    CPPA_REQUIRE(reason != exit_reason::not_exited);
    // move everyhting out of the critical section before processing it
    decltype(m_links) mlinks;
    decltype(m_attachables) mattachables;
    { // lifetime scope of guard
        guard_type guard{m_mtx};
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
    CPPA_LOGC_INFO_IF(not is_proxy(), "cppa::actor", __func__,
                      "actor with ID " << m_id << " had " << mlinks.size()
                      << " links and " << mattachables.size()
                      << " attached functors; exit reason = " << reason
                      << ", class = " << detail::demangle(typeid(*this)));
    // send exit messages
    auto msg = make_any_tuple(exit_msg{address(), reason});
    CPPA_LOGM_DEBUG("cppa::actor", "send EXIT to " << mlinks.size() << " links");
    for (auto& aptr : mlinks) {
        aptr->enqueue({address(), aptr, message_id{}.with_high_priority()},
                      msg, m_host);
    }
    CPPA_LOGM_DEBUG("cppa::actor", "run " << mattachables.size()
                                   << " attachables");
    for (attachable_ptr& ptr : mattachables) {
        ptr->actor_exited(reason);
    }
}

std::set<std::string> abstract_actor::interface() const {
    // defaults to untyped
    return std::set<std::string>{};
}

} // namespace cppa
