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


#ifndef CPPA_SCHEDULED_ACTOR_HPP
#define CPPA_SCHEDULED_ACTOR_HPP

#include <iostream>

#include <atomic>

#include "cppa/any_tuple.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/detail/abstract_actor.hpp"
#include "cppa/scheduled_actor.hpp"

#include "cppa/util/fiber.hpp"
#include "cppa/detail/filter_result.hpp"
#include "cppa/detail/recursive_queue_node.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"


namespace cppa { namespace detail {

// A spawned, scheduled Actor.
class abstract_scheduled_actor : public abstract_actor<scheduled_actor> {

    typedef abstract_actor<scheduled_actor> super;

 protected:

    std::atomic<int> m_state;

    filter_result filter_msg(const any_tuple& msg) {
        auto& arr = detail::static_types_array<atom_value, std::uint32_t>::arr;
        if (   msg.size() == 2
            && msg.type_at(0) == arr[0]
            && msg.type_at(1) == arr[1]) {
            auto v0 = *reinterpret_cast<const atom_value*>(msg.at(0));
            auto v1 = *reinterpret_cast<const std::uint32_t*>(msg.at(1));
            if (v0 == atom("EXIT")) {
                if (this->m_trap_exit == false) {
                    if (v1 != exit_reason::normal) {
                        quit(v1);
                    }
                    return normal_exit_signal;
                }
            }
            else if (v0 == atom("TIMEOUT")) {
                return (v1 == m_active_timeout_id) ? timeout_message
                                                   : expired_timeout_message;
            }
        }
        return ordinary_message;
    }

    bool has_pending_timeout() {
        return m_has_pending_timeout_request;
    }

    void request_timeout(const util::duration& d) {
        if (d.valid()) {
            if (d.is_zero()) {
                // immediately enqueue timeout
                enqueue(nullptr, make_any_tuple(atom("TIMEOUT"),
                                                ++m_active_timeout_id));
            }
            else {
                get_scheduler()->delayed_send(this, d, atom("TIMEOUT"),
                                              ++m_active_timeout_id);
                m_has_pending_timeout_request = true;
            }
        }
    }

    void reset_timeout() {
        if (m_has_pending_timeout_request) {
            ++m_active_timeout_id;
            m_has_pending_timeout_request = false;
        }
    }

    inline void handle_timeout(behavior& bhvr) {
        bhvr.handle_timeout();
        reset_timeout();
    }

    inline void push_timeout() {
        ++m_active_timeout_id;
    }

    inline void pop_timeout() {
        --m_active_timeout_id;
    }

    bool m_has_pending_timeout_request;
    std::uint32_t m_active_timeout_id;

 public:

    static constexpr int ready          = 0x00;
    static constexpr int done           = 0x01;
    static constexpr int blocked        = 0x02;
    static constexpr int pending        = 0x03;
    static constexpr int about_to_block = 0x04;

    abstract_scheduled_actor(int state = done)
        : super(true), m_state(state)
        , m_has_pending_timeout_request(false)
        , m_active_timeout_id(0) {
    }

    bool pending_enqueue(actor* sender, any_tuple msg) {
        return enqueue_node(super::fetch_node(sender, std::move(msg)), pending);
    }

    void quit(std::uint32_t reason = exit_reason::normal) {
        this->cleanup(reason);
        throw actor_exited(reason);
    }

    void enqueue(actor* sender, any_tuple msg) {
        enqueue_node(super::fetch_node(sender, std::move(msg)));
    }

    int compare_exchange_state(int expected, int new_value) {
        int e = expected;
        do {
            if (m_state.compare_exchange_weak(e, new_value)) {
                return new_value;
            }
        }
        while (e == expected);
        return e;
    }

 private:

    bool enqueue_node(typename super::mailbox_element* node,
                      int next_state = ready) {
        CPPA_REQUIRE(node->marked == false);
        if (this->m_mailbox._push_back(node)) {
            for (;;) {
                int state = m_state.load();
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
        }
        return false;
    }

};

} } // namespace cppa::detail

#endif // CPPA_SCHEDULED_ACTOR_HPP
