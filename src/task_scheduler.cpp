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


#include <iostream>

#include "cppa/config.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/task_scheduler.hpp"
#include "cppa/detail/yielding_actor.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/converted_thread_context.hpp"

using std::cout;
using std::cerr;
using std::endl;

namespace {

void enqueue_fun(cppa::detail::task_scheduler* where,
                 cppa::detail::abstract_scheduled_actor* what)
{
    where->schedule(what);
}

} // namespace <anonmyous>

namespace cppa { namespace detail {

void task_scheduler::worker_loop(job_queue* jq, abstract_scheduled_actor* dummy)
{
    cppa::util::fiber fself;
    abstract_scheduled_actor* job = nullptr;
    struct handler : abstract_scheduled_actor::resume_callback
    {
        abstract_scheduled_actor** job_ptr;
        handler(abstract_scheduled_actor** jptr) : job_ptr(jptr) { }
        bool still_ready()
        {
            return true;
        }
        void exec_done()
        {
            auto job = *job_ptr;
            if (!job->deref()) delete job;
            CPPA_MEMORY_BARRIER();
            dec_actor_count();
        }
    };
    handler h(&job);
    for (;;)
    {
        job = jq->pop();
        if (job == dummy)
        {
            return;
        }
        job->resume(&fself, &h);
    }
}

void task_scheduler::start()
{
    super::start();
    m_worker = thread(worker_loop, &m_queue, &m_dummy);
}

void task_scheduler::stop()
{
    m_queue.push_back(&m_dummy);
    m_worker.join();
    super::stop();
}

void task_scheduler::schedule(abstract_scheduled_actor* what)
{
    if (what)
    {
        if (this_thread::get_id() == m_worker.get_id())
        {
            m_queue._push_back(what);
        }
        else
        {
            m_queue.push_back(what);
        }
    }
}

actor_ptr task_scheduler::spawn_impl(abstract_scheduled_actor* what)
{
    inc_actor_count();
    CPPA_MEMORY_BARRIER();
    intrusive_ptr<abstract_scheduled_actor> ctx(what);
    // add an implicit reference to ctx
    ctx->ref();
    m_queue.push_back(ctx.get());
    return std::move(ctx);
}


actor_ptr task_scheduler::spawn(abstract_event_based_actor* what)
{
    return spawn_impl(what->attach_to_scheduler(enqueue_fun, this));
}

#ifndef CPPA_DISABLE_CONTEXT_SWITCHING
actor_ptr task_scheduler::spawn(scheduled_actor* bhvr, scheduling_hint hint)
{
    if (hint == detached) return mock_scheduler::spawn(bhvr);
    return spawn_impl(new yielding_actor(bhvr, enqueue_fun, this));
}
#else
actor_ptr task_scheduler::spawn(scheduled_actor* bhvr, scheduling_hint)
{
    return mock_scheduler::spawn(bhvr);
}
#endif

} } // namespace cppa::detail
