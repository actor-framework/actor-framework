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


#include "cppa/on.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/event_based_actor.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

namespace cppa {

scheduled_actor::scheduled_actor(actor_state init_state, bool chained_send)
: super(chained_send), next(nullptr), m_has_pending_tout(false)
, m_pending_tout(0), m_state(init_state), m_scheduler(nullptr)
, m_hidden(false) { }

void scheduled_actor::attach_to_scheduler(scheduler* sched, bool hidden) {
    CPPA_REQUIRE(sched != nullptr);
    m_scheduler = sched;
    m_hidden = hidden;
    // init is called by the spawning actor, manipulate self to
    // point to this actor
    scoped_self_setter sss{this};
    // initialize this actor
    try { init(); }
    catch (...) { }
    // make sure scheduler is not set until init() is done
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

bool scheduled_actor::initialized() const {
    return m_scheduler != nullptr;
}

void scheduled_actor::request_timeout(const util::duration& d) {
    if (!d.valid()) m_has_pending_tout = false;
    else {
        auto msg = make_any_tuple(atom("SYNC_TOUT"), ++m_pending_tout);
        if (d.is_zero()) {
            // immediately enqueue timeout message if duration == 0s
            auto e = this->new_mailbox_element(this, std::move(msg));
            this->m_mailbox.enqueue(e);
        }
        else get_scheduler()->delayed_send(this, d, std::move(msg));
        m_has_pending_tout = true;
    }
}

scheduled_actor::~scheduled_actor() {
    if (!m_mailbox.closed()) {
        detail::sync_request_bouncer f{exit_reason()};
        m_mailbox.close(f);
    }
}

void scheduled_actor::run_detached() {
    throw std::logic_error("scheduled_actor::run_detached called");
}

void scheduled_actor::cleanup(std::uint32_t reason) {
    detail::sync_request_bouncer f{reason};
    m_mailbox.close(f);
    super::cleanup(reason);
}

bool scheduled_actor::enqueue_impl(actor_state next_state,
                                   const message_header& hdr,
                                   any_tuple&& msg) {
    CPPA_REQUIRE(   next_state == actor_state::ready
                 || next_state == actor_state::pending);
    auto e = new_mailbox_element(hdr, std::move(msg));
    switch (m_mailbox.enqueue(e)) {
        case intrusive::first_enqueued: {
            auto state = m_state.load();
            for (;;) {
                switch (state) {
                    case actor_state::blocked: {
                        if (m_state.compare_exchange_weak(state, next_state)) {
                            CPPA_REQUIRE(m_scheduler != nullptr);
                            if (next_state == actor_state::ready) {
                                CPPA_LOG_DEBUG("enqueued actor with id " << id()
                                               << " to job queue");
                                m_scheduler->enqueue(this);
                                return false;
                            }
                            return true;
                        }
                        break;
                    }
                    case actor_state::about_to_block: {
                        if (m_state.compare_exchange_weak(state, actor_state::ready)) {
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
            if (hdr.id.is_request()) {
                detail::sync_request_bouncer f{exit_reason()};
                f(hdr.sender, hdr.id);
            }
            break;
        }
        default: break;
    }
    return false;
}

actor_state scheduled_actor::compare_exchange_state(actor_state expected,
                                                    actor_state desired) {
    auto e = expected;
    do { if (m_state.compare_exchange_weak(e, desired)) return desired; }
    while (e == expected);
    return e;
}

void scheduled_actor::enqueue(const message_header& hdr, any_tuple msg) {
    enqueue_impl(actor_state::ready, hdr, std::move(msg));
}

bool scheduled_actor::chained_enqueue(const message_header& hdr, any_tuple msg) {
    return enqueue_impl(actor_state::pending, hdr, std::move(msg));
}

} // namespace cppa
