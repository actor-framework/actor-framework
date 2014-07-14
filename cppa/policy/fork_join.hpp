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

#ifndef CPPA_POLICY_FORK_JOIN_HPP
#define CPPA_POLICY_FORK_JOIN_HPP

#include <chrono>
#include <vector>
#include <thread>

#include "cppa/resumable.hpp"

#include "cppa/detail/producer_consumer_list.hpp"

namespace cppa {
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
     *        operations. Note that we do dequeue from the back of the
     *        queue.
     */
    using priv_queue = std::vector<resumable*>;

    template<class Worker>
    inline void external_enqueue(Worker*, resumable* job) {
        m_exposed_queue.push_back(job);
    }

    template<class Worker>
    inline void internal_enqueue(Worker*, resumable* job) {
        // give others the opportunity to steal from us
        if (m_exposed_queue.empty()) {
            if (m_private_queue.empty()) {
                m_exposed_queue.push_back(job);
            } else {
                m_exposed_queue.push_back(m_private_queue.front());
                m_private_queue.erase(m_private_queue.begin());
                m_private_queue.push_back(job);
            }
        } else {
            m_private_queue.push_back(job);
        }
    }

    template<class Worker>
    inline resumable* try_external_dequeue(Worker*) {
        return m_exposed_queue.try_pop();
    }

    template<class Worker>
    inline resumable* internal_dequeue(Worker* self) {
        resumable* job;
        auto local_poll = [&]() -> bool {
            if (!m_private_queue.empty()) {
                job = m_private_queue.back();
                m_private_queue.pop_back();
                return true;
            }
            return false;
        };
        auto aggressive_poll = [&]() -> bool {
            for (int i = 1; i < 101; ++i) {
                job = m_exposed_queue.try_pop();
                if (job) {
                    return true;
                }
                // try to steal every 10 poll attempts
                if ((i % 10) == 0) {
                    job = self->raid();
                    if (job) {
                        return true;
                    }
                }
                std::this_thread::yield();
            }
            return false;
        };
        auto moderate_poll = [&]() -> bool {
            for (int i = 1; i < 550; ++i) {
                job = m_exposed_queue.try_pop();
                if (job) {
                    return true;
                }
                // try to steal every 5 poll attempts
                if ((i % 5) == 0) {
                    job = self->raid();
                    if (job) {
                        return true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            return false;
        };
        auto relaxed_poll = [&]() -> bool {
            for (;;) {
                job = m_exposed_queue.try_pop();
                if (job) {
                    return true;
                }
                // always try to steal at this stage
                job = self->raid();
                if (job) {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        };
        local_poll() || aggressive_poll() || moderate_poll() || relaxed_poll();
        return job;
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
            m_private_queue.erase(m_private_queue.begin());
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

    // this queue is exposed to others, i.e., other workers
    // may attempt to steal jobs from it and the central scheduling
    // unit can push new jobs to the queue
    sync_queue m_exposed_queue;

    // internal job queue
    priv_queue m_private_queue;

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_FORK_JOIN_HPP
