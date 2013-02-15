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
#include "cppa/atom.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"

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
            m_observer->enqueue(m_observed.get(),
                                make_any_tuple(atom("DOWN"), reason));
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

} // namespace <anonymous>

local_actor::local_actor(bool sflag)
: m_chaining(sflag), m_trap_exit(false)
, m_is_scheduled(sflag), m_dummy_node(), m_current_node(&m_dummy_node) { }

void local_actor::monitor(actor_ptr whom) {
    if (whom) whom->attach(new down_observer(this, whom));
}

void local_actor::demonitor(actor_ptr whom) {
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

std::vector<group_ptr> local_actor::joined_groups() {
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
        send_message(whom.get(), std::move(what));
    }
    else if (!id.is_answered()) {
        if (chaining_enabled()) {
            if (whom->chained_sync_enqueue(this, id.response_id(), std::move(what))) {
                m_chained_actor = whom;
            }
        }
        else {
            whom->sync_enqueue(this, id.response_id(), std::move(what));
        }
        id.mark_as_answered();
    }
}

void local_actor::forward_message(const actor_ptr& new_receiver) {
    if (new_receiver == nullptr) {
        return;
    }
    auto& from = last_sender();
    auto id = m_current_node->mid;
    if (id.valid() == false || id.is_response()) {
        new_receiver->enqueue(from.get(), m_current_node->msg);
    }
    else {
        new_receiver->sync_enqueue(from.get(), id, m_current_node->msg);
        // treat this message as asynchronous message from now on
        m_current_node->mid = message_id_t();
    }
}

sync_recv_helper local_actor::handle_response(const message_future& handle) {
    return {handle.id(), [](behavior& bhvr, message_id_t mf) {
        if (!self->awaits(mf)) {
            throw std::logic_error("response already received");
        }
        self->become_waiting_for(std::move(bhvr), mf);
    }};
}

response_handle local_actor::make_response_handle() {
    auto n = m_current_node;
    response_handle result(this, n->sender, n->mid.response_id());
    n->mid.mark_as_answered();
    return std::move(result);
}

message_id_t local_actor::send_timed_sync_message(actor* whom,
                                                  const util::duration& rel_time,
                                                  any_tuple&& what) {
    auto mid = this->send_sync_message(whom, std::move(what));
    auto tmp = make_any_tuple(atom("TIMEOUT"));
    get_scheduler()->delayed_reply(this, rel_time, mid, std::move(tmp));
    return mid;
}

void local_actor::exec_behavior_stack() {
    // default implementation does nothing
}

} // namespace cppa
