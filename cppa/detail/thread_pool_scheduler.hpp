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


#ifndef THREAD_POOL_SCHEDULER_HPP
#define THREAD_POOL_SCHEDULER_HPP

#include "cppa/scheduler.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/util/producer_consumer_list.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa { namespace detail {

class thread_pool_scheduler : public scheduler
{

    typedef scheduler super;

 public:

    struct worker;

    void start() /*override*/;

    void stop() /*override*/;

    void enqueue(abstract_scheduled_actor* what) /*override*/;

    actor_ptr spawn(abstract_event_based_actor* what);

    actor_ptr spawn(scheduled_actor* behavior, scheduling_hint hint);

 private:

    //typedef util::single_reader_queue<abstract_scheduled_actor> job_queue;
    typedef util::producer_consumer_list<abstract_scheduled_actor> job_queue;

    job_queue m_queue;
    scheduled_actor_dummy m_dummy;
    thread m_supervisor;

    actor_ptr spawn_impl(abstract_scheduled_actor* what,
                         bool push_to_queue = true);

    static void worker_loop(worker*);
    static void supervisor_loop(job_queue*, abstract_scheduled_actor*);

};

} } // namespace cppa::detail

#endif // THREAD_POOL_SCHEDULER_HPP
