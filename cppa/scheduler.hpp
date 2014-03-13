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


#ifndef CPPA_SCHEDULER_HPP
#define CPPA_SCHEDULER_HPP

#include <chrono>
#include <memory>
#include <thread>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/attachable.hpp"
#include "cppa/scoped_actor.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/execution_unit.hpp"
#include "cppa/message_header.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/producer_consumer_list.hpp"

namespace cppa {

class resumable;

namespace detail { class singleton_manager; }

namespace scheduler {

class worker;

/**
 * @brief Central scheduling interface.
 */
class coordinator {

    friend class detail::singleton_manager;

 public:

    class shutdown_helper;

    /**
     * @brief Returns a handle to the central printing actor.
     */
    actor printer() const;

    /**
     * @brief Puts @p what into the queue of a randomly chosen worker.
     */
    void enqueue(resumable* what);

    template<typename Duration, typename... Data>
    void delayed_send(message_header hdr,
                      const Duration& rel_time,
                      any_tuple data           ) {
        auto tup = make_any_tuple(atom("SEND"),
                                  util::duration{rel_time},
                                  std::move(hdr),
                                  std::move(data));
        m_timer->enqueue(message_header{}, std::move(tup), nullptr);
    }

    template<typename Duration, typename... Data>
    void delayed_reply(message_header hdr,
                       const Duration& rel_time,
                       any_tuple data           ) {
        CPPA_REQUIRE(hdr.id.valid() && hdr.id.is_response());
        auto tup = make_any_tuple(atom("SEND"),
                                  util::duration{rel_time},
                                  std::move(hdr),
                                  std::move(data));
        m_timer->enqueue(message_header{}, std::move(tup), nullptr);
    }

    inline size_t num_workers() const {
        return static_cast<unsigned>(m_workers.size());
    }

    inline worker* worker_by_id(size_t id) const {
        return m_workers[id].get();
    }

 private:

    static coordinator* create_singleton();

    coordinator();

    inline void dispose() { delete this; }

    void initialize();

    void destroy();

    intrusive_ptr<blocking_actor> m_timer;
    scoped_actor m_printer;

    std::thread m_timer_thread;
    std::thread m_printer_thread;

    // ID of the worker receiving the next enqueue
    std::atomic<size_t> m_next_worker;

    // vector of size std::thread::hardware_concurrency()
    std::vector<std::unique_ptr<worker>> m_workers;

};

/**
 * @brief A work-stealing scheduling worker.
 *
 * The work-stealing implementation of libcppa minimizes access to the
 * synchronized queue. The reasoning behind this design decision is that
 * it has been shown that stealing actually is very rare for workloads [1].
 * Hence, implementations should focus on the performance in
 * the non-stealing case. For this reason, each worker has an exposed
 * job queue that can be accessed by the central scheduler instance as
 * well as other workers, but it also has a private job list it is
 * currently working on. To account for the load balancing aspect, each
 * worker makes sure that at least one job is left in its exposed queue
 * to allow other workers to steal it.
 *
 * [1] http://dl.acm.org/citation.cfm?doid=2398857.2384639
 */
class worker : public execution_unit {

    friend class coordinator;

    friend class coordinator::shutdown_helper;

 public:

    typedef resumable* job_ptr;

    typedef util::producer_consumer_list<resumable> job_queue;

    worker(size_t id, coordinator* parent);

    /**
     * @brief Attempt to steal an element from the exposed job queue.
     */
    job_ptr try_steal();

    /**
     * @brief Enqueues a new job to the worker's queue from an external
     *        source, i.e., from any other thread.
     */
    void external_enqueue(job_ptr);

    /**
     * @brief Enqueues a new job to the worker's queue from an internal
     *        source, i.e., a job that is currently executed by
     *        this worker.
     * @warning Must not be called from other threads.
     */
    void exec_later(job_ptr) override;

 private:

    void start(); // called from the scheduler

    void run(); // work loop

    job_ptr raid(); // go on a raid in quest for a new shiny job

    bool m_running;

    // this queue is exposed to others, i.e., other workers
    // may attempt to steal jobs from it and the central scheduling
    // unit can push new jobs to the queue
    job_queue m_exposed_queue;

    // internal job stack
    std::vector<job_ptr> m_job_list;

    // the worker's thread
    std::thread m_this_thread;

    // the worker's ID received from scheduler
    size_t m_id;

    // the ID of the last victim we stole from
    size_t m_last_victim;

    coordinator* m_parent;

};

} // namespace scheduler

} // namespace cppa

#endif // CPPA_SCHEDULER_HPP
