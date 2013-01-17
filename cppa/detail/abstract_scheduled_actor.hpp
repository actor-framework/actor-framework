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
#include "cppa/detail/recursive_queue_node.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace detail {

// A spawned, scheduled Actor.
class abstract_scheduled_actor : public abstract_actor<scheduled_actor> {

    typedef abstract_actor<scheduled_actor> super;

 protected:

    std::atomic<int> m_state;

    inline bool has_pending_timeout() const {
        return m_has_pending_timeout_request;
    }

    void request_timeout(const util::duration& d);

    inline void reset_timeout() {
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
        CPPA_REQUIRE(m_active_timeout_id > 0);
        --m_active_timeout_id;
    }

    inline bool waits_for_timeout(std::uint32_t timeout_id) {
        return    m_has_pending_timeout_request
               && m_active_timeout_id == timeout_id;
    }

    bool m_has_pending_timeout_request;
    std::uint32_t m_active_timeout_id;

 public:

    static constexpr int ready          = 0x00;
    static constexpr int done           = 0x01;
    static constexpr int blocked        = 0x02;
    static constexpr int pending        = 0x03;
    static constexpr int about_to_block = 0x04;

    abstract_scheduled_actor(int state = done);

    bool chained_enqueue(actor* sender, any_tuple msg);

    bool chained_sync_enqueue(actor* sender, message_id_t id, any_tuple msg);

    void quit(std::uint32_t reason = exit_reason::normal);

    void enqueue(actor* sender, any_tuple msg);

    void sync_enqueue(actor* sender, message_id_t id, any_tuple msg);

    int compare_exchange_state(int expected, int new_value);

 private:

    bool enqueue_node(recursive_queue_node* node,
                      int next_state = ready,
                      bool* failed = nullptr);

};

} } // namespace cppa::detail

#endif // CPPA_SCHEDULED_ACTOR_HPP
