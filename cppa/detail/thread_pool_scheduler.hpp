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


#ifndef CPPA_THREAD_POOL_SCHEDULER_HPP
#define CPPA_THREAD_POOL_SCHEDULER_HPP

#include <thread>

#include "cppa/scheduler.hpp"
#include "cppa/context_switching_actor.hpp"
#include "cppa/util/producer_consumer_list.hpp"
#include "cppa/detail/scheduled_actor_dummy.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa { namespace detail {

class thread_pool_scheduler : public scheduler {

    typedef scheduler super;

 public:

    struct worker;

    void start() /*override*/;

    void stop() /*override*/;

    void enqueue(scheduled_actor* what) /*override*/;

    actor_ptr spawn(scheduled_actor* what);

    actor_ptr spawn(scheduled_actor* what, init_callback init_cb);

    actor_ptr spawn(void_function fun, scheduling_hint hint);

    actor_ptr spawn(void_function what,
                    scheduling_hint hint,
                    init_callback init_cb);

 private:

    //typedef util::single_reader_queue<abstract_scheduled_actor> job_queue;
    typedef util::producer_consumer_list<scheduled_actor> job_queue;

    job_queue m_queue;
    scheduled_actor_dummy m_dummy;
    std::thread m_supervisor;

    static void worker_loop(worker*);
    static void supervisor_loop(job_queue*, scheduled_actor*);

    actor_ptr spawn_impl(scheduled_actor_ptr what);

    actor_ptr spawn_as_thread(void_function fun, init_callback cb, bool hidden);

#   ifndef CPPA_DISABLE_CONTEXT_SWITCHING
    template<typename F>
    actor_ptr spawn_impl(void_function fun, scheduling_hint sh, F cb) {
        if (sh == scheduled) {
            scheduled_actor_ptr ptr{new context_switching_actor(std::move(fun))};
            ptr->attach_to_scheduler(this);
            cb(ptr.get());
            return spawn_impl(std::move(ptr));
        }
        else {
            return spawn_as_thread(std::move(fun), std::move(cb));
        }
    }
#   endif
};

} } // namespace cppa::detail

#endif // CPPA_THREAD_POOL_SCHEDULER_HPP
