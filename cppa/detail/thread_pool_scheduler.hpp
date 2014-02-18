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


#ifndef CPPA_THREAD_POOL_SCHEDULER_HPP
#define CPPA_THREAD_POOL_SCHEDULER_HPP

#include <thread>

#include "cppa/scheduler.hpp"

#include "cppa/util/producer_consumer_list.hpp"

#include "cppa/detail/resumable.hpp"

namespace cppa { namespace detail {

struct cs_thread;

class thread_pool_scheduler : public scheduler {

    typedef scheduler super;

 public:

    struct dummy : resumable {
        resume_result resume(detail::cs_thread*) override;
    };

    struct worker;

    thread_pool_scheduler();

    thread_pool_scheduler(size_t num_worker_threads);

    void initialize();

    void destroy();

    void enqueue(resumable* what) override;

 private:

    //typedef util::single_reader_queue<abstract_scheduled_actor> job_queue;
    typedef util::producer_consumer_list<resumable> job_queue;

    size_t m_num_threads;
    job_queue m_queue;
    dummy m_dummy;
    std::thread m_supervisor;

    static void worker_loop(worker*);
    static void supervisor_loop(job_queue*, resumable*, size_t);

};

} } // namespace cppa::detail

#endif // CPPA_THREAD_POOL_SCHEDULER_HPP
