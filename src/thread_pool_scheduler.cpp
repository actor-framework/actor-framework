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


#include <cstdint>
#include <cstddef>
#include <iostream>

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/yielding_actor.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

using std::cout;
using std::endl;

namespace cppa { namespace detail {

namespace {

void enqueue_fun(cppa::detail::thread_pool_scheduler* where,
                 cppa::detail::abstract_scheduled_actor* what)
{
    where->schedule(what);
}

typedef unique_lock<mutex> guard_type;
typedef std::unique_ptr<thread_pool_scheduler::worker> worker_ptr;
typedef util::single_reader_queue<thread_pool_scheduler::worker> worker_queue;

} // namespace <anonmyous>

struct thread_pool_scheduler::worker
{

    typedef abstract_scheduled_actor* job_ptr;

    job_queue* m_job_queue;
    job_ptr m_dummy;
    thread m_thread;

    worker(job_queue* jq, job_ptr dummy) : m_job_queue(jq), m_dummy(dummy)
    {
    }

    void start()
    {
        m_thread = thread(&thread_pool_scheduler::worker_loop, this);
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    job_ptr aggressive_polling()
    {
        job_ptr result = nullptr;
        for (int i = 0; i < 3; ++i)
        {
            result = m_job_queue->try_pop();
            if (result)
            {
                return result;
            }
            detail::this_thread::yield();
        }
        return result;
    }

    job_ptr less_aggressive_polling()
    {
        job_ptr result = nullptr;
        for (int i = 0; i < 10; ++i)
        {
            result =  m_job_queue->try_pop();
            if (result)
            {
                return result;
            }
#           ifdef __APPLE__
            auto timeout = boost::get_system_time();
            timeout += boost::posix_time::milliseconds(1);
            boost::this_thread::sleep(timeout);
#           else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
#           endif
        }
        return result;
    }

    job_ptr relaxed_polling()
    {
        job_ptr result = nullptr;
        for (;;)
        {
            result =  m_job_queue->try_pop();
            if (result)
            {
                return result;
            }
#           ifdef __APPLE__
            auto timeout = boost::get_system_time();
            timeout += boost::posix_time::milliseconds(10);
            boost::this_thread::sleep(timeout);
#           else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
#           endif
        }
    }

    void operator()()
    {
        util::fiber fself;
        struct handler : abstract_scheduled_actor::resume_callback
        {
            abstract_scheduled_actor* job;
            handler() : job(nullptr) { }
            bool still_ready() { return true; }
            void exec_done()
            {
                if (!job->deref()) delete job;
                dec_actor_count();
                job = nullptr;
            }
        };
        handler h;
        for (;;)
        {
            h.job = aggressive_polling();
            if (!h.job)
            {
                h.job = less_aggressive_polling();
                if (!h.job)
                {
                    h.job = relaxed_polling();
                }
            }
            if (h.job == m_dummy)
            {
                // dummy of doom received ...
                m_job_queue->push_back(h.job); // kill the next guy
                return;                        // and say goodbye
            }
            else
            {
                h.job->resume(&fself, &h);
            }
        }
    }

};

void thread_pool_scheduler::worker_loop(thread_pool_scheduler::worker* w)
{
    (*w)();
}

void thread_pool_scheduler::supervisor_loop(job_queue* jqueue,
                                            abstract_scheduled_actor* dummy)
{
    std::vector<worker_ptr> workers;
    size_t num_workers = std::max<size_t>(thread::hardware_concurrency() * 2, 4);
    for (size_t i = 0; i < num_workers; ++i)
    {
        worker_ptr wptr(new worker(jqueue, dummy));
        wptr->start();
        workers.push_back(std::move(wptr));
    }
    // wait for workers
    for (auto& w : workers)
    {
        w->m_thread.join();
    }
}

void thread_pool_scheduler::start()
{
    m_supervisor = thread(&thread_pool_scheduler::supervisor_loop,
                          &m_queue, &m_dummy);
    super::start();
}

void thread_pool_scheduler::stop()
{
    m_queue.push_back(&m_dummy);
    m_supervisor.join();
    super::stop();
}

void thread_pool_scheduler::schedule(abstract_scheduled_actor* what)
{
    m_queue.push_back(what);
}

actor_ptr thread_pool_scheduler::spawn_impl(abstract_scheduled_actor* what,
                                            bool push_to_queue)
{
    inc_actor_count();
    CPPA_MEMORY_BARRIER();
    intrusive_ptr<abstract_scheduled_actor> ctx(what);
    ctx->ref();
    if (push_to_queue) m_queue.push_back(ctx.get());
    return std::move(ctx);
}


actor_ptr thread_pool_scheduler::spawn(abstract_event_based_actor* what)
{
    // do NOT push event-based actors to the queue on startup
    return spawn_impl(what->attach_to_scheduler(enqueue_fun, this), false);
}

#ifndef CPPA_DISABLE_CONTEXT_SWITCHING
actor_ptr thread_pool_scheduler::spawn(scheduled_actor* bhvr,
                                       scheduling_hint hint)
{
    if (hint == detached)
    {
        return mock_scheduler::spawn(bhvr);
    }
    else
    {
        return spawn_impl(new yielding_actor(bhvr, enqueue_fun, this));
    }
}
#else
actor_ptr thread_pool_scheduler::spawn(scheduled_actor* bhvr, scheduling_hint)
{
    return mock_scheduler::spawn(bhvr);
}
#endif

} } // namespace cppa::detail
