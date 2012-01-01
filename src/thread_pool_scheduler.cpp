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

    worker* next;
    bool m_done;
    job_queue* m_job_queue;
    volatile abstract_scheduled_actor* m_job;
    worker_queue* m_supervisor_queue;
    mutex m_mtx;
    condition_variable m_cv;
    thread m_thread;

    worker(worker_queue* supervisor_queue, job_queue* jq)
        : next(nullptr), m_done(false), m_job_queue(jq), m_job(nullptr)
        , m_supervisor_queue(supervisor_queue)
    {
    }

    void start()
    {
        m_thread = thread(&thread_pool_scheduler::worker_loop, this);
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    void operator()()
    {
        typedef decltype(now()) time_type;
        // enqueue as idle worker
        m_supervisor_queue->push_back(this);
        util::fiber fself;
        struct handler : abstract_scheduled_actor::resume_callback
        {
            time_type timeout;
            bool reschedule;
            abstract_scheduled_actor* job;
            handler() : timeout(now()), reschedule(false), job(nullptr)
            {
            }
            bool still_ready()
            {
                if (timeout >= now())
                {
                    reschedule = true;
                    return false;
                }
                return true;
            }
            void exec_done()
            {
                if (!job->deref()) delete job;
                CPPA_MEMORY_BARRIER();
                dec_actor_count();
                job = nullptr;
            }
        };
        handler h;
        for (;;)
        {
            // lifetime scope of guard (wait for new job)
            {
                guard_type guard(m_mtx);
                while (m_job == nullptr && !m_done)
                {
                    m_cv.wait(guard);
                }
                if (m_done) return;
            }
            h.job = const_cast<abstract_scheduled_actor*>(m_job);
            // run actor up to 300ms
            h.reschedule = false;
            h.timeout = now();
            h.timeout += std::chrono::milliseconds(300);
            h.job->resume(&fself, &h);
            if (h.reschedule && h.job)
            {
                m_job_queue->push_back(h.job);
            }
            m_job = nullptr;
            CPPA_MEMORY_BARRIER();
            m_supervisor_queue->push_back(this);
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
    worker_queue wqueue;
    std::vector<worker_ptr> workers;
    // init with at least two workers
    size_t num_workers = std::max<size_t>(thread::hardware_concurrency(), 2);
    auto new_worker = [&]()
    {
        worker_ptr wptr(new worker(&wqueue, jqueue));
        wptr->start();
        workers.push_back(std::move(wptr));
    };
    for (size_t i = 0; i < num_workers; ++i)
    {
        new_worker();
    }
    bool done = false;
    // loop
    do
    {
        // fetch next job
        abstract_scheduled_actor* job = jqueue->pop();
        if (job == dummy)
        {
            done = true;
        }
        else
        {
            // fetch next idle worker (wait up to 500ms)
            worker* w = nullptr;
            auto timeout = now();
            timeout += std::chrono::milliseconds(500);
            while (!w)
            {
                w = wqueue.try_pop(timeout);
                // all workers are blocked since 500ms, start a new one
                if (!w)
                {
                    new_worker();
                }
            }
            // lifetime scope of guard
            {
                guard_type guard(w->m_mtx);
                w->m_job = job;
                w->m_cv.notify_one();
            }
        }
    }
    while (!done);
    // quit
    for (auto& w : workers)
    {
        guard_type guard(w->m_mtx);
        w->m_done = true;
        w->m_cv.notify_one();
    }
    // wait for workers
    for (auto& w : workers)
    {
        w->m_thread.join();
    }
    // "clear" worker_queue
    while (wqueue.try_pop() != nullptr) { }
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

actor_ptr thread_pool_scheduler::spawn_impl(abstract_scheduled_actor* what)
{
    inc_actor_count();
    CPPA_MEMORY_BARRIER();
    intrusive_ptr<abstract_scheduled_actor> ctx(what);
    ctx->ref();
    m_queue.push_back(ctx.get());
    return std::move(ctx);
}


actor_ptr thread_pool_scheduler::spawn(abstract_event_based_actor* what)
{
    return spawn_impl(what->attach_to_scheduler(enqueue_fun, this));
}

actor_ptr thread_pool_scheduler::spawn(scheduled_actor* behavior,
                                       scheduling_hint hint)
{
    if (hint == detached)
    {
        return mock_scheduler::spawn(behavior);
    }
    else
    {
        return spawn_impl(new yielding_actor(behavior,
                                             enqueue_fun,
                                             this));
    }
}

} } // namespace cppa::detail
