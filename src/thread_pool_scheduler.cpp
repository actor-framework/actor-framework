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


#include <mutex>
#include <thread>
#include <cstdint>
#include <cstddef>
#include <iostream>

#include "cppa/abstract_event_based_actor.hpp"

#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/context_switching_actor.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

using std::cout;
using std::endl;

namespace cppa { namespace detail {

namespace {

typedef std::unique_lock<std::mutex> guard_type;
typedef intrusive::single_reader_queue<thread_pool_scheduler::worker> worker_queue;

} // namespace <anonmyous>

struct thread_pool_scheduler::worker {

    typedef scheduled_actor* job_ptr;

    job_queue* m_job_queue;
    job_ptr m_dummy;
    std::thread m_thread;

    worker(job_queue* jq, job_ptr dummy) : m_job_queue(jq), m_dummy(dummy) {
    }

    void start() {
        m_thread = std::thread(&thread_pool_scheduler::worker_loop, this);
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    job_ptr aggressive_polling() {
        job_ptr result = nullptr;
        for (int i = 0; i < 3; ++i) {
            result = m_job_queue->try_pop();
            if (result) {
                return result;
            }
            std::this_thread::yield();
        }
        return result;
    }

    job_ptr less_aggressive_polling() {
        job_ptr result = nullptr;
        for (int i = 0; i < 10; ++i) {
            result =  m_job_queue->try_pop();
            if (result) {
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return result;
    }

    job_ptr relaxed_polling() {
        job_ptr result = nullptr;
        for (;;) {
            result =  m_job_queue->try_pop();
            if (result) {
                return result;
            }
#           ifdef CPPA_USE_BOOST_THREADS
            auto timeout = boost::get_system_time();
            timeout += boost::posix_time::milliseconds(10);
            boost::this_thread::sleep(timeout);
#           else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
#           endif
        }
    }

    void operator()() {
        util::fiber fself;
        scheduled_actor* job = nullptr;
        auto fetch_pending = [&job]() -> scheduled_actor* {
            CPPA_REQUIRE(job != nullptr);
            scheduled_actor* result = nullptr;
            if (job->chained_actor() != nullptr) {
                result = static_cast<scheduled_actor*>(job->chained_actor().release());
            }
            return result;
        };
        for (;;) {
            job = aggressive_polling();
            if (job == nullptr) {
                job = less_aggressive_polling();
                if (job == nullptr) {
                    job = relaxed_polling();
                }
            }
            if (job == m_dummy) {
                // dummy of doom received ...
                m_job_queue->push_back(job); // kill the next guy
                return;                        // and say goodbye
            }
            else {
                do {
                    switch (job->resume(&fself)) {
                        case resume_result::actor_done: {
                            scheduled_actor* pending = fetch_pending();
                            if (!job->deref()) delete job;
                            std::atomic_thread_fence(std::memory_order_seq_cst);
                            dec_actor_count();
                            job = pending;
                            break;
                        }
                        case resume_result::actor_blocked: {
                            job = fetch_pending();
                        }
                    }
                }
                while (job);
            }
        }
    }

};

void thread_pool_scheduler::worker_loop(thread_pool_scheduler::worker* w) {
    (*w)();
}

void thread_pool_scheduler::supervisor_loop(job_queue* jqueue,
                                            scheduled_actor* dummy) {
    typedef std::unique_ptr<thread_pool_scheduler::worker> worker_ptr;
    std::vector<worker_ptr> workers;
    size_t num_workers = std::max<size_t>(std::thread::hardware_concurrency() * 2, 8);
    for (size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back(new worker(jqueue, dummy));
        workers.back()->start();
    }
    // wait for workers
    for (auto& w : workers) {
        w->m_thread.join();
    }
}

void thread_pool_scheduler::start() {
    m_supervisor = std::thread(&thread_pool_scheduler::supervisor_loop,
                               &m_queue, &m_dummy);
    super::start();
}

void thread_pool_scheduler::stop() {
    m_queue.push_back(&m_dummy);
    m_supervisor.join();
    super::stop();
}

void thread_pool_scheduler::enqueue(scheduled_actor* what) {
    m_queue.push_back(what);
}

actor_ptr thread_pool_scheduler::spawn_impl(scheduled_actor* what,
                                            bool push_to_queue) {
    inc_actor_count();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    intrusive_ptr<scheduled_actor> ctx(what);
    ctx->ref();
    if (push_to_queue) m_queue.push_back(ctx.get());
    return std::move(ctx);
}


actor_ptr thread_pool_scheduler::spawn(scheduled_actor* what) {
    // do NOT push event-based actors to the queue on startup
    return spawn_impl(what->attach_to_scheduler(this), false);
}

#ifndef CPPA_DISABLE_CONTEXT_SWITCHING
actor_ptr thread_pool_scheduler::spawn(std::function<void()> what,
                                       scheduling_hint hint) {
    if (hint == scheduled) {
        auto new_actor = new context_switching_actor(std::move(what));
        return spawn_impl(new_actor->attach_to_scheduler(this));
    }
    else {
        return mock_scheduler::spawn_impl(std::move(what));
    }
}
#else
actor_ptr thread_pool_scheduler::spawn(std::function<void()> what,
                                       scheduling_hint) {
    return mock_scheduler::spawn_impl(std::move(what));
}
#endif

} } // namespace cppa::detail
