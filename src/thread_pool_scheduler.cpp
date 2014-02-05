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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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

#include "cppa/on.hpp"
#include "cppa/logging.hpp"

#include "cppa/util/fiber.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"

using std::cout;
using std::endl;

namespace cppa { namespace detail {

resumable::resume_result thread_pool_scheduler::dummy::resume(util::fiber*) {
    throw std::logic_error("thread_pool_scheduler::dummy::resume");
}

struct thread_pool_scheduler::worker {

    typedef resumable* job_ptr;

    job_queue* m_job_queue;
    job_ptr m_dummy;
    std::thread m_thread;

    worker(job_queue* jq, job_ptr dummy) : m_job_queue(jq), m_dummy(dummy) { }

    void start() {
        m_thread = std::thread(&thread_pool_scheduler::worker_loop, this);
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    bool aggressive(job_ptr& result) {
        for (int i = 0; i < 100; ++i) {
            result = m_job_queue->try_pop();
            if (result) return true;
            std::this_thread::yield();
        }
        return false;
    }

    bool moderate(job_ptr& result) {
        for (int i = 0; i < 550; ++i) {
            result =  m_job_queue->try_pop();
            if (result) return true;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        return false;
    }

    bool relaxed(job_ptr& result) {
        for (;;) {
            result =  m_job_queue->try_pop();
            if (result) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void operator()() {
        CPPA_LOG_TRACE("");
        util::fiber fself;
        job_ptr job = nullptr;
        for (;;) {
            aggressive(job) || moderate(job) || relaxed(job);
            CPPA_LOG_DEBUG("dequeued new job");
            if (job == m_dummy) {
                CPPA_LOG_DEBUG("received dummy (quit)");
                // dummy of doom received ...
                m_job_queue->push_back(job); // kill the next guy
                return;                      // and say goodbye
            }
            if (job->resume(&fself) == resumable::done) {
                CPPA_LOG_DEBUG("actor is done");
                /*FIXME bool hidden = job->is_hidden();
                job->deref();
                if (!hidden)*/ get_actor_registry()->dec_running();
            }
            job = nullptr;
        }
    }

};

void thread_pool_scheduler::worker_loop(thread_pool_scheduler::worker* w) {
    (*w)();
}

thread_pool_scheduler::thread_pool_scheduler() {
    m_num_threads = std::max<size_t>(std::thread::hardware_concurrency(), 4);
}

thread_pool_scheduler::thread_pool_scheduler(size_t num_worker_threads) {
    m_num_threads = num_worker_threads;
}

void thread_pool_scheduler::supervisor_loop(job_queue* jqueue,
                                            resumable* dummy,
                                            size_t num_threads) {
    std::vector<std::unique_ptr<thread_pool_scheduler::worker> > workers;
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(new worker(jqueue, dummy));
        workers.back()->start();
    }
    // wait for workers
    for (auto& w : workers) {
        w->m_thread.join();
    }
}

void thread_pool_scheduler::initialize() {
    m_supervisor = std::thread(&thread_pool_scheduler::supervisor_loop,
                               &m_queue, &m_dummy, m_num_threads);
    super::initialize();
}

void thread_pool_scheduler::destroy() {
    CPPA_LOG_TRACE("");
    m_queue.push_back(&m_dummy);
    CPPA_LOG_DEBUG("join supervisor");
    m_supervisor.join();
    // make sure job queue is empty, because destructor of m_queue would
    // otherwise delete elements it shouldn't
    CPPA_LOG_DEBUG("flush queue");
    auto ptr = m_queue.try_pop();
    while (ptr != nullptr) {
        if (ptr != &m_dummy) {
            /*FIXME bool hidden = ptr->is_hidden();
            ptr->deref();
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if (!hidden)*/ get_actor_registry()->dec_running();
        }
        ptr = m_queue.try_pop();
    }
    super::destroy();
}

void thread_pool_scheduler::enqueue(resumable* what) {
    m_queue.push_back(what);
}

} } // namespace cppa::detail
