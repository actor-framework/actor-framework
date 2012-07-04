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


#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/local_actor.hpp"

namespace cppa {

namespace {

class down_observer : public attachable {

    actor_ptr m_observer;
    actor_ptr m_observed;

 public:

    down_observer(actor_ptr observer, actor_ptr observed)
        : m_observer(std::move(observer))
        , m_observed(std::move(observed)) {
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
    if (whom) {
        auto response_id = get_response_id();
        if (response_id == 0) {
            send_message(whom.get(), std::move(what));
        }
        else {
            if (m_chaining && !m_chained_actor) {
                if (whom->chained_sync_enqueue(this,
                                               response_id,
                                               std::move(what))) {
                    m_chained_actor = whom;
                }
            }
            else whom->sync_enqueue(this, response_id, std::move(what));
        }
    }
}

} // namespace cppa
