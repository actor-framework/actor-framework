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

#include "cppa/event_based_actor.hpp"
#include "cppa/thread_mapped_actor.hpp"

#include "cppa/detail/actor_count.hpp"
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

    worker(job_queue* jq, job_ptr dummy) : m_job_queue(jq), m_dummy(dummy) { }

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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void operator()() {
        util::fiber fself;
        job_ptr job = nullptr;
        auto fetch_pending = [&job]() -> job_ptr {
            CPPA_REQUIRE(job != nullptr);
            auto ptr = job->chained_actor().release();
            return ptr ? static_cast<scheduled_actor*>(ptr) : nullptr;
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
                return;                      // and say goodbye
            }
            else {
                do {
                    switch (job->resume(&fself)) {
                        case resume_result::actor_done: {
                            auto pending = fetch_pending();
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
    std::vector<std::unique_ptr<thread_pool_scheduler::worker> > workers;
    size_t num_workers = std::max<size_t>(std::thread::hardware_concurrency() * 2, 4);
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
    // make sure job queue is empty, because destructor of m_queue would
    // otherwise delete elements it shouldn't
    auto ptr = m_queue.try_pop();
    while (ptr != nullptr) {
        if (ptr != &m_dummy) {
            if (!ptr->deref()) delete ptr;
            std::atomic_thread_fence(std::memory_order_seq_cst);
            dec_actor_count();
            ptr = m_queue.try_pop();
        }
    }
    super::stop();
}

void thread_pool_scheduler::enqueue(scheduled_actor* what) {
    m_queue.push_back(what);
}

actor_ptr thread_pool_scheduler::spawn_as_thread(void_function fun,
                                                 init_callback cb,
                                                 bool hidden) {
    if (!hidden) inc_actor_count();
    thread_mapped_actor_ptr ptr{new thread_mapped_actor(std::move(fun))};
    ptr->init();
    ptr->initialized(true);
    cb(ptr.get());
    std::thread([hidden, ptr]() {
        scoped_self_setter sss{ptr.get()};
        try {
            ptr->run();
            ptr->on_exit();
        }
        catch (...) { }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (!hidden) dec_actor_count();
    }).detach();
    return ptr;
}

actor_ptr thread_pool_scheduler::spawn_impl(scheduled_actor_ptr what) {
    if (what->has_behavior()) {
        inc_actor_count();
        what->ref();
        // event-based actors are not pushed to the job queue on startup
        if (what->impl_type() == context_switching_impl) {
            m_queue.push_back(what.get());
        }
    }
    else {
        what->on_exit();
    }
    return std::move(what);
}

actor_ptr thread_pool_scheduler::spawn(scheduled_actor* raw) {
    scheduled_actor_ptr ptr{raw};
    ptr->attach_to_scheduler(this);
    return spawn_impl(std::move(ptr));
}

actor_ptr thread_pool_scheduler::spawn(scheduled_actor* raw, init_callback cb) {
    scheduled_actor_ptr ptr{raw};
    ptr->attach_to_scheduler(this);
    cb(ptr.get());
    return spawn_impl(std::move(ptr));
}

#ifndef CPPA_DISABLE_CONTEXT_SWITCHING
actor_ptr thread_pool_scheduler::spawn(void_function fun, scheduling_hint sh) {
    if (sh == scheduled) {
        scheduled_actor_ptr ptr{new context_switching_actor(std::move(fun))};
        ptr->attach_to_scheduler(this);
        return spawn_impl(std::move(ptr));
    }
    else {
        return spawn_as_thread(std::move(fun),
                               [](local_actor*) { },
                               sh == detached_and_hidden);
    }
}
actor_ptr thread_pool_scheduler::spawn(void_function fun,
                                       scheduling_hint sh,
                                       init_callback init_cb) {
    if (sh == scheduled) {
        scheduled_actor_ptr ptr{new context_switching_actor(std::move(fun))};
        ptr->attach_to_scheduler(this);
        init_cb(ptr.get());
        return spawn_impl(std::move(ptr));
    }
    else {
        return spawn_as_thread(std::move(fun), std::move(init_cb), sh == detached_and_hidden);
    }
}
#else
actor_ptr thread_pool_scheduler::spawn(void_function what, scheduling_hint) {
    return spawn_as_thread(std::move(what), [](local_actor*) { });
}
actor_ptr thread_pool_scheduler::spawn(void_function what,
                                       scheduling_hint,
                                       init_callback init_cb) {
    return spawn_as_thread(std::move(what), std::move(init_cb));
}
#endif

} } // namespace cppa::detail
