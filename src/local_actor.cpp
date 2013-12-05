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


#include <string>
#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/logging.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/message_future.hpp"

#include "cppa/detail/raw_access.hpp"

namespace cppa {

namespace {

class down_observer : public attachable {

    actor_addr m_observer;
    actor_addr m_observed;

 public:

    down_observer(actor_addr observer, actor_addr observed)
    : m_observer(std::move(observer)), m_observed(std::move(observed)) {
        CPPA_REQUIRE(m_observer != nullptr);
        CPPA_REQUIRE(m_observed != nullptr);
    }

    void actor_exited(std::uint32_t reason) {
        if (m_observer) {
            auto ptr = detail::actor_addr_cast<abstract_actor>(m_observer);
            message_header hdr{m_observed, ptr, message_priority::high};
            hdr.deliver(make_any_tuple(atom("DOWN"), reason));
        }
    }

    bool matches(const attachable::token& match_token) {
        if (match_token.subtype == typeid(down_observer)) {
            auto ptr = reinterpret_cast<const local_actor*>(match_token.ptr);
            return m_observer == ptr->address();
        }
        return false;
    }

};

constexpr const char* s_default_debug_name = "actor";

} // namespace <anonymous>

local_actor::local_actor(bool sflag)
: m_trap_exit(false)
, m_is_scheduled(sflag), m_dummy_node(), m_current_node(&m_dummy_node)
, m_planned_exit_reason(exit_reason::not_exited) {
#   ifdef CPPA_DEBUG_MODE
    new (&m_debug_name) std::string (std::to_string(m_id) + "@local");
#   endif // CPPA_DEBUG_MODE
}

local_actor::~local_actor() {
    using std::string;
#   ifdef CPPA_DEBUG_MODE
    m_debug_name.~string();
#   endif // CPPA_DEBUG_MODE
}

const char* local_actor::debug_name() const {
#   ifdef CPPA_DEBUG_MODE
    return m_debug_name.c_str();
#   else // CPPA_DEBUG_MODE
    return s_default_debug_name;
#   endif // CPPA_DEBUG_MODE
}

void local_actor::debug_name(std::string str) {
#   ifdef CPPA_DEBUG_MODE
    m_debug_name = std::move(str);
#   else // CPPA_DEBUG_MODE
    CPPA_LOG_WARNING("unable to set debug name to " << str
                     << " (compiled without debug mode enabled)");
#   endif // CPPA_DEBUG_MODE
}

void local_actor::monitor(const actor_addr& whom) {
    if (!whom) return;
    auto ptr = detail::actor_addr_cast<abstract_actor>(whom);
    ptr->attach(attachable_ptr{new down_observer(address(), whom)});
}

void local_actor::demonitor(const actor_addr& whom) {
    if (!whom) return;
    auto ptr = detail::actor_addr_cast<abstract_actor>(whom);
    attachable::token mtoken{typeid(down_observer), this};
    ptr->detach(mtoken);
}

void local_actor::on_exit() { }

void local_actor::init() { }

void local_actor::join(const group_ptr& what) {
    if (what && m_subscriptions.count(what) == 0) {
        m_subscriptions.insert(std::make_pair(what, what->subscribe(this)));
    }
}

void local_actor::leave(const group_ptr& what) {
    if (what) m_subscriptions.erase(what);
}

std::vector<group_ptr> local_actor::joined_groups() const {
    std::vector<group_ptr> result;
    for (auto& kvp : m_subscriptions) {
        result.emplace_back(kvp.first);
    }
    return result;
}

void local_actor::reply_message(any_tuple&& what) {
    auto& whom = m_current_node->sender;
    if (!whom) return;
    auto& id = m_current_node->mid;
    if (id.valid() == false || id.is_response()) {
        send_tuple(detail::actor_addr_cast<abstract_actor>(whom), std::move(what));
    }
    else if (!id.is_answered()) {
        auto ptr = detail::actor_addr_cast<abstract_actor>(whom);
        ptr->enqueue({address(), ptr, id.response_id()}, std::move(what));
        id.mark_as_answered();
    }
}

void local_actor::forward_message(const actor& dest, message_priority p) {
    if (!dest) return;
    auto& id = m_current_node->mid;
    detail::raw_access::get(dest)->enqueue({m_current_node->sender, detail::raw_access::get(dest), id, p}, m_current_node->msg);
    // treat this message as asynchronous message from now on
    id = message_id{};
}

/* TODO:
response_handle local_actor::make_response_handle() {
    auto n = m_current_node;
    response_handle result{this, n->sender, n->mid.response_id()};
    n->mid.mark_as_answered();
    return result;
}
*/

void local_actor::send_tuple(const channel& dest, any_tuple what) {
    //TODO:
}

void local_actor::remove_handler(message_id) {

}

void local_actor::delayed_send_tuple(const channel&, const util::duration&, cppa::any_tuple) {

}

message_future local_actor::timed_sync_send_tuple(const util::duration& rtime,
                                                  const actor& dest,
                                                  any_tuple what) {

}

response_handle local_actor::make_response_handle() {
    return {};
}

void local_actor::cleanup(std::uint32_t reason) {
    m_subscriptions.clear();
    super::cleanup(reason);
}

void local_actor::quit(std::uint32_t reason) {
    CPPA_LOG_TRACE("reason = " << reason
                   << ", class " << detail::demangle(typeid(*this)));
    if (reason == exit_reason::unallowed_function_call) {
        // this is the only reason that causes an exception
        cleanup(reason);
        CPPA_LOG_WARNING("actor tried to use a blocking function");
        // when using receive(), the non-blocking nature of event-based
        // actors breaks any assumption the user has about his code,
        // in particular, receive_loop() is a deadlock when not throwing
        // an exception here
        aout << "*** warning: event-based actor killed because it tried to "
                "use receive()\n";
        throw actor_exited(reason);
    }
    planned_exit_reason(reason);
}

} // namespace cppa
