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
#include "cppa/config.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa { namespace detail {

namespace {
void dummy_enqueue(void*, abstract_scheduled_actor*) { }
types_array<atom_value, std::uint32_t> t_atom_ui32_types;
}

abstract_scheduled_actor::abstract_scheduled_actor(scheduler* sched)
    : next(nullptr)
    , m_state(ready)
    , m_scheduler(sched)
    , m_has_pending_timeout_request(false)
    , m_active_timeout_id(0) {
    CPPA_REQUIRE(sched != nullptr);
}

abstract_scheduled_actor::abstract_scheduled_actor(int state)
    : next(nullptr)
    , m_state(state)
    , m_scheduler(nullptr)
    , m_has_pending_timeout_request(false)
    , m_active_timeout_id(0) {
}

abstract_scheduled_actor::resume_callback::~resume_callback() {
}

void abstract_scheduled_actor::quit(std::uint32_t reason) {
    cleanup(reason);
    throw actor_exited(reason);
}

void abstract_scheduled_actor::enqueue_node(queue_node* node) {
    if (m_mailbox._push_back(node)) {
        for (;;) {
            int state = m_state.load();
            switch (state) {
                case blocked: {
                    if (m_state.compare_exchange_weak(state, ready)) {
                        CPPA_REQUIRE(m_scheduler != nullptr);
                        m_scheduler->enqueue(this);
                        return;
                    }
                    break;
                }
                case about_to_block: {
                    if (m_state.compare_exchange_weak(state, ready)) {
                        return;
                    }
                    break;
                }
                default: return;
            }
        }
    }
}

void abstract_scheduled_actor::enqueue(actor* sender, any_tuple&& msg) {
    enqueue_node(fetch_node(sender, std::move(msg)));
    //enqueue_node(new queue_node(sender, std::move(msg)));
}

void abstract_scheduled_actor::enqueue(actor* sender, const any_tuple& msg) {
    enqueue_node(fetch_node(sender, msg));
    //enqueue_node(new queue_node(sender, msg));
}

int abstract_scheduled_actor::compare_exchange_state(int expected,
                                                     int new_value) {
    int e = expected;
    do {
        if (m_state.compare_exchange_weak(e, new_value)) {
            return new_value;
        }
    }
    while (e == expected);
    return e;
}

void abstract_scheduled_actor::request_timeout(const util::duration& d) {
    if (d.valid()) {
        future_send(this, d, atom(":Timeout"), ++m_active_timeout_id);
        m_has_pending_timeout_request = true;
    }
}

auto abstract_scheduled_actor::filter_msg(const any_tuple& msg) -> filter_result {
    if (   msg.size() == 2
        && msg.type_at(0) == t_atom_ui32_types[0]
        && msg.type_at(1) == t_atom_ui32_types[1]) {
        auto v0 = *reinterpret_cast<const atom_value*>(msg.at(0));
        auto v1 = *reinterpret_cast<const std::uint32_t*>(msg.at(1));
        if (v0 == atom(":Exit")) {
            if (m_trap_exit == false) {
                if (v1 != exit_reason::normal) {
                    quit(v1);
                }
                return normal_exit_signal;
            }
        }
        else if (v0 == atom(":Timeout")) {
            return (v1 == m_active_timeout_id) ? timeout_message
                                               : expired_timeout_message;
        }
    }
    return ordinary_message;
}

auto abstract_scheduled_actor::dq(queue_node& node,
                                  partial_function& fun) -> dq_result {
    CPPA_REQUIRE(node.msg.cvals().get() != nullptr);
    if (node.marked) return dq_indeterminate;
    switch (filter_msg(node.msg)) {
        case normal_exit_signal:
        case expired_timeout_message: {
            // skip message
            return dq_indeterminate;
        }
        case timeout_message: {
            // m_active_timeout_id is already invalid
            m_has_pending_timeout_request = false;
            return dq_timeout_occured;
        }
        default: break;
    }
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    //m_last_dequeued = node.msg;
    //m_last_sender = node.sender;
    // make sure no timeout is handled incorrectly in a nested receive
    ++m_active_timeout_id;
    // lifetime scope of qguard {
        // make sure nested receives do not process this node again
        queue_node_guard qguard{&node};
        // try to invoke given function
        if (fun(m_last_dequeued)) {
            // client erases node later (keep it marked until it's removed)
            qguard.release();
            // this members are only valid during invocation
            m_last_dequeued.reset();
            m_last_sender.reset();
            // we definitely don't have a pending timeout now
            m_has_pending_timeout_request = false;
            return dq_done;
        }
    }
    // no match, restore members
    --m_active_timeout_id;
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    return dq_indeterminate;
}

// dummy

void scheduled_actor_dummy::resume(util::fiber*, resume_callback*) {
}

void scheduled_actor_dummy::quit(std::uint32_t) {
}

void scheduled_actor_dummy::dequeue(behavior&) {
}

void scheduled_actor_dummy::dequeue(partial_function&) {
}

void scheduled_actor_dummy::link_to(intrusive_ptr<actor>&) {
}

void scheduled_actor_dummy::unlink_from(intrusive_ptr<actor>&) {
}

bool scheduled_actor_dummy::establish_backlink(intrusive_ptr<actor>&) {
    return false;
}

bool scheduled_actor_dummy::remove_backlink(intrusive_ptr<actor>&) {
    return false;
}

void scheduled_actor_dummy::detach(const attachable::token&) {
}

bool scheduled_actor_dummy::attach(attachable*) {
    return false;
}

} } // namespace cppa::detail
