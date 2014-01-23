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
            message_header hdr{m_observed, ptr, message_id{}.with_high_priority()};
            hdr.deliver(make_any_tuple(down_msg{m_observed, reason}));
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

local_actor::local_actor()
: m_trap_exit(false)
, m_dummy_node(), m_current_node(&m_dummy_node)
, m_planned_exit_reason(exit_reason::not_exited)
, m_state(actor_state::ready) {
    m_node = node_id::get();
}

local_actor::~local_actor() { }

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
    auto id = (p == message_priority::high)
            ? m_current_node->mid.with_high_priority()
            : m_current_node->mid.with_normal_priority();
    detail::raw_access::get(dest)->enqueue({m_current_node->sender, detail::raw_access::get(dest), id}, m_current_node->msg);
    // treat this message as asynchronous message from now on
    m_current_node->mid = message_id::invalid;
}

void local_actor::send_tuple(message_priority prio, const channel& dest, any_tuple what) {
    message_id id;
    if (prio == message_priority::high) id = id.with_high_priority();
    dest.enqueue({address(), dest, id}, std::move(what));
}

void local_actor::send_tuple(const channel& dest, any_tuple what) {
    send_tuple(message_priority::normal, dest, std::move(what));
}

void local_actor::send_exit(const actor_addr& whom, std::uint32_t reason) {
    send(detail::raw_access::get(whom), exit_msg{address(), reason});
}

void local_actor::delayed_send_tuple(const channel& dest,
                                     const util::duration& rel_time,
                                     cppa::any_tuple msg) {
    get_scheduler()->delayed_send({address(), dest}, rel_time, std::move(msg));
}

response_promise local_actor::make_response_promise() {
    auto n = m_current_node;
    response_promise result{address(), n->sender, n->mid.response_id()};
    n->mid.mark_as_answered();
    return result;
}

void local_actor::cleanup(std::uint32_t reason) {
    CPPA_LOG_TRACE(CPPA_ARG(reason));
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

message_id local_actor::timed_sync_send_tuple_impl(message_priority mp,
                                                   const actor& dest,
                                                   const util::duration& rtime,
                                                   any_tuple&& what) {
    auto nri = new_request_id();
    if (mp == message_priority::high) nri = nri.with_high_priority();
    dest.enqueue({address(), dest, nri}, std::move(what));
    auto rri = nri.response_id();
    get_scheduler()->delayed_send({address(), this, rri}, rtime,
                                  make_any_tuple(sync_timeout_msg{}));
    return rri;
}

} // namespace cppa
