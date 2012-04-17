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
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa { namespace detail {

class yielding_actor : public abstract_scheduled_actor
{

    typedef abstract_scheduled_actor super;
    typedef super::queue_node queue_node;
    typedef super::queue_node_ptr queue_node_ptr;

    util::fiber m_fiber;
    scheduled_actor* m_behavior;

    static void run(void* _this);

    void exec_loop_stack();

    void yield_until_not_empty();

 public:

    yielding_actor(scheduled_actor* behavior, scheduler* sched);

    ~yielding_actor(); //override

    void dequeue(behavior& bhvr); //override

    void dequeue(partial_function& fun); //override

    void resume(util::fiber* from, resume_callback* callback); //override

 private:

    template<typename Fun>
    void dequeue_impl(Fun rm_fun)
    {
        auto& mbox_cache = m_mailbox.cache();
        auto mbox_end = mbox_cache.end();
        auto iter = std::find_if(mbox_cache.begin(), mbox_end, rm_fun);
        while (iter == mbox_end)
        {
            yield_until_not_empty();
            iter = std::find_if(m_mailbox.try_fetch_more(), mbox_end, rm_fun);
        }
        mbox_cache.erase(iter);
    }

};

} } // namespace cppa::detail

#endif // CPPA_DISABLE_CONTEXT_SWITCHING

#endif // YIELDING_ACTOR_HPP
