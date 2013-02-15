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


#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa { namespace detail {

void abstract_scheduled_actor::request_timeout(const util::duration& d) {
    if (d.valid()) {
        if (d.is_zero()) {
            // immediately enqueue timeout
            auto node = super::fetch_node(this,
                                          make_any_tuple(atom("SYNC_TOUT"),
                                          ++m_active_timeout_id));
            this->m_mailbox.enqueue(node);
        }
        else {
            get_scheduler()->delayed_send(
                        this, d,
                        make_any_tuple(
                            atom("SYNC_TOUT"), ++m_active_timeout_id));
        }
        m_has_pending_timeout_request = true;
    }
    else m_has_pending_timeout_request = false;
}

abstract_scheduled_actor::abstract_scheduled_actor(int state)
: super(true), m_state(state), m_has_pending_timeout_request(false)
, m_active_timeout_id(0) {
}

bool abstract_scheduled_actor::chained_enqueue(actor* sender, any_tuple msg) {
    return enqueue_node(super::fetch_node(sender, std::move(msg)), pending);
}

bool abstract_scheduled_actor::chained_sync_enqueue(actor* sender, message_id_t id, any_tuple msg) {
    bool failed;
    bool result = enqueue_node(super::fetch_node(sender, std::move(msg), id), pending, &failed);
    if (failed) {
        sync_request_bouncer f{this, exit_reason()};
        f(sender, id);
    }
    return result;
}

void abstract_scheduled_actor::quit(std::uint32_t reason) {
    this->cleanup(reason);
    throw actor_exited(reason);
}

void abstract_scheduled_actor::enqueue(actor* sender, any_tuple msg) {
    enqueue_node(super::fetch_node(sender, std::move(msg)));
}

void abstract_scheduled_actor::sync_enqueue(actor* sender, message_id_t id, any_tuple msg) {
    bool failed;
    enqueue_node(super::fetch_node(sender, std::move(msg), id), ready, &failed);
    if (failed) {
        sync_request_bouncer f{this, exit_reason()};
        f(sender, id);
    }
}

int abstract_scheduled_actor::compare_exchange_state(int expected, int new_value) {
    int e = expected;
    do {
        if (m_state.compare_exchange_weak(e, new_value)) {
            return new_value;
        }
    }
    while (e == expected);
    return e;
}

bool abstract_scheduled_actor::enqueue_node(recursive_queue_node* node,
                                            int next_state,
                                            bool* failed) {
    CPPA_REQUIRE(next_state == ready || next_state == pending);
    CPPA_REQUIRE(node->marked == false);
    switch (this->m_mailbox.enqueue(node)) {
        case intrusive::first_enqueued: {
            int state = m_state.load();
            for (;;) {
                switch (state) {
                    case blocked: {
                        if (m_state.compare_exchange_weak(state, next_state)) {
                            CPPA_REQUIRE(this->m_scheduler != nullptr);
                            if (next_state == ready) {
                                this->m_scheduler->enqueue(this);
                            }
                            return true;
                        }
                        break;
                    }
                    case about_to_block: {
                        if (m_state.compare_exchange_weak(state, ready)) {
                            return false;
                        }
                        break;
                    }
                    default: return false;
                }
            }
            break;
        }
        case intrusive::queue_closed: {
            if (failed) *failed = true;
            break;
        }
        default: break;
    }
    return false;
}

} } // namespace cppa::detail
