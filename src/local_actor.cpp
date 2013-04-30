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

namespace cppa {

namespace {

class down_observer : public attachable {

    actor_ptr m_observer;
    actor_ptr m_observed;

 public:

    down_observer(actor_ptr observer, actor_ptr observed)
    : m_observer(std::move(observer)), m_observed(std::move(observed)) {
        CPPA_REQUIRE(m_observer != nullptr);
        CPPA_REQUIRE(m_observed != nullptr);
    }

    void actor_exited(std::uint32_t reason) {
        if (m_observer) {
            message_header hdr{m_observed, m_observer, message_priority::high};
            hdr.deliver(make_any_tuple(atom("DOWN"), reason));
        }
    }

    bool matches(const attachable::token& match_token) {
        if (match_token.subtype == typeid(down_observer)) {
            auto ptr = reinterpret_cast<const local_actor*>(match_token.ptr);
            return m_observer == ptr;
        }
        return false;
    }

};

constexpr const char* s_default_debug_name = "actor";

} // namespace <anonymous>

local_actor::local_actor(bool sflag)
: m_chaining(sflag), m_trap_exit(false)
, m_is_scheduled(sflag), m_dummy_node(), m_current_node(&m_dummy_node) {
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

void local_actor::monitor(const actor_ptr& whom) {
    if (whom) whom->attach(attachable_ptr{new down_observer(this, whom)});
}

void local_actor::demonitor(const actor_ptr& whom) {
    attachable::token mtoken{typeid(down_observer), this};
    if (whom) whom->detach(mtoken);
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
    auto& whom = last_sender();
    if (whom == nullptr) {
        return;
    }
    auto& id = m_current_node->mid;
    if (id.valid() == false || id.is_response()) {
        send_tuple(whom, std::move(what));
    }
    else if (!id.is_answered()) {
        if (chaining_enabled()) {
            if (whom->chained_enqueue({this, whom, id.response_id()}, std::move(what))) {
                chained_actor(whom);
            }
        }
        else whom->enqueue({this, whom, id.response_id()}, std::move(what));
        id.mark_as_answered();
    }
}

void local_actor::forward_message(const actor_ptr& new_receiver) {
    if (new_receiver == nullptr) return;
    auto& id = m_current_node->mid;
    new_receiver->enqueue({last_sender(), new_receiver, id}, m_current_node->msg);
    // treat this message as asynchronous message from now on
    id = message_id{};
}

response_handle local_actor::make_response_handle() {
    auto n = m_current_node;
    response_handle result{this, n->sender, n->mid.response_id()};
    n->mid.mark_as_answered();
    return std::move(result);
}

void local_actor::exec_behavior_stack() {
    // default implementation does nothing
}

void local_actor::cleanup(std::uint32_t reason) {
    m_subscriptions.clear();
    super::cleanup(reason);
}

} // namespace cppa
