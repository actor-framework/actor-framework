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


#ifndef YIELDING_ACTOR_HPP
#define YIELDING_ACTOR_HPP

#include "cppa/config.hpp"

#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <stack>

#include "cppa/either.hpp"
#include "cppa/pattern.hpp"

#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/nestable_receive_policy.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa { namespace detail {

class yielding_actor : public abstract_scheduled_actor {

    friend class nestable_receive_policy;

    typedef abstract_scheduled_actor super;

 public:

    yielding_actor(std::function<void()> fun);

    void dequeue(behavior& bhvr); //override

    void dequeue(partial_function& fun); //override

    void resume(util::fiber* from, scheduler::callback* callback); //override

    inline void push_timeout() {
        ++m_active_timeout_id;
    }

    inline void pop_timeout() {
        --m_active_timeout_id;
    }

 private:

    typedef std::unique_ptr<recursive_queue_node> queue_node_ptr;

    void run();

    static void trampoline(void* _this);

    void yield_until_not_empty();

    util::fiber m_fiber;
    std::function<void()> m_behavior;
    nestable_receive_policy m_recv_policy;

    inline recursive_queue_node* receive_node() {
        recursive_queue_node* e = m_mailbox.try_pop();
        while (e == nullptr) {
            yield_until_not_empty();
            e = m_mailbox.try_pop();
        }
        return e;
    }

};

} } // namespace cppa::detail

#endif // CPPA_DISABLE_CONTEXT_SWITCHING

#endif // YIELDING_ACTOR_HPP
