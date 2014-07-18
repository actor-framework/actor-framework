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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_POLICY_FORK_JOIN_HPP
#define CAF_POLICY_FORK_JOIN_HPP

#include <deque>
#include <chrono>
#include <thread>
#include <cstddef>

#include "caf/resumable.hpp"

#include "caf/detail/producer_consumer_list.hpp"

namespace caf {
namespace policy {

/**
 * @brief An implementation of the {@link job_queue_policy} concept for
 *        fork-join like processing of actors.
 *
 * This work-stealing fork-join implementation uses two queues: a
 * synchronized queue accessible by other threads and an internal queue.
 * Access to the synchronized queue is minimized.
 * The reasoning behind this design decision is that it
 * has been shown that stealing actually is very rare for most workloads [1].
 * Hence, implementations should focus on the performance in
 * the non-stealing case. For this reason, each worker has an exposed
 * job queue that can be accessed by the central scheduler instance as
 * well as other workers, but it also has a private job list it is
 * currently working on. To account for the load balancing aspect, each
 * worker makes sure that at least one job is left in its exposed queue
 * to allow other workers to steal it.
 *
 * @relates job_queue_policy
 */
class fork_join {

 public:

    fork_join() = default;

    fork_join(fork_join&& other) {
        // delegate to move assignment operator
        *this = std::move(other);
    }

    fork_join& operator=(fork_join&& other) {
        m_private_queue = std::move(other.m_private_queue);
        auto next = [&] { return other.m_exposed_queue.try_pop(); };
        for (auto j = next(); j != nullptr; j = next()) {
            m_exposed_queue.push_back(j);
        }
        return *this;
    }

    /**
     * @brief A thead-safe queue implementation.
     */
    using sync_queue = detail::producer_consumer_list<resumable>;

    /**
     * @brief A queue implementation supporting fast push and pop
     *        operations on both ends of the queue.
     */
    using priv_queue = std::deque<resumable*>;

    template<class Worker>
    void external_enqueue(Worker*, resumable* job) {
        m_exposed_queue.push_back(job);
    }

    template<class Worker>
    void internal_enqueue(Worker* ptr, resumable* job) {
        m_exposed_queue.push_back(job);
        // give others the opportunity to steal from us
        assert_stealable(ptr);
    }

    template<class Worker>
    resumable* try_external_dequeue(Worker*) {
        return m_exposed_queue.try_pop();
    }

    template<class Worker>
    resumable* internal_dequeue(Worker* self) {
        // we wait for new jobs by polling our external queue: first, we
        // assume an active work load on the machine and perform aggresive
        // polling, then we relax our polling a bit and wait 50 us between
        // dequeue attempts, finally we assume pretty much nothing is going
        // on and poll every 10 ms; this strategy strives to minimize the
        // downside of "busy waiting", which still performs much better than a
        // "signalizing" implementation based on mutexes and conition variables
        struct poll_strategy {
            size_t attempts;
            size_t step_size;
            size_t raid_interval;
            std::chrono::microseconds sleep_duration;
        };
        constexpr poll_strategy strategies[3] = {
            // aggressive polling  (100x) without sleep interval
            {100, 1, 10, std::chrono::microseconds{0}},
            // moderate polling (500x) with 50 us sleep interval
            {500, 1, 5,  std::chrono::microseconds{50}},
            // relaxed polling (infinite attempts) with 10 ms sleep interval
            {101, 0, 1,  std::chrono::microseconds{10000}}
        };
        resumable* job = nullptr;
        // local poll
        if (!m_private_queue.empty()) {
            job = m_private_queue.back();
            m_private_queue.pop_back();
            return job;
        }
        for (auto& strat : strategies) {
            for (size_t i = 0; i < strat.attempts; i += strat.step_size) {
                job = m_exposed_queue.try_pop();
                if (job) {
                    return job;
                }
                // try to steal every X poll attempts
                if ((i % strat.raid_interval) == 0) {
                    job = self->raid();
                    if (job) {
                        return job;
                    }
                }
                std::this_thread::sleep_for(strat.sleep_duration);
            }
        }
        // unreachable, because the last strategy loops
        // until a job has been dequeued
        return nullptr;
    }

    template<class Worker>
    void clear_internal_queue(Worker*) {
        // give others the opportunity to steal unfinished jobs
        for (auto ptr : m_private_queue) {
            m_exposed_queue.push_back(ptr);
        }
        m_private_queue.clear();
    }

    template<class Worker>
    void assert_stealable(Worker*) {
        // give others the opportunity to steal from us
        if (m_private_queue.size() > 1 && m_exposed_queue.empty()) {
            m_exposed_queue.push_back(m_private_queue.front());
            m_private_queue.pop_front();
        }
    }

    template<class Worker, typename UnaryFunction>
    void consume_all(Worker*, UnaryFunction f) {
        for (auto job : m_private_queue) {
            f(job);
        }
        m_private_queue.clear();
        auto next = [&] { return m_exposed_queue.try_pop(); };
        for (auto job = next(); job != nullptr; job = next()) {
            f(job);
        }
    }

 private:

    // this queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue
    sync_queue m_exposed_queue;

    // internal job queue
    priv_queue m_private_queue;

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_FORK_JOIN_HPP
