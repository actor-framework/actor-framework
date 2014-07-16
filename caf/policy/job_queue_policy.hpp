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

#ifndef CAF_POLICY_JOB_QUEUE_POLICY_HPP
#define CAF_POLICY_JOB_QUEUE_POLICY_HPP

#include "caf/fwd.hpp"

namespace caf {
namespace policy {

/**
 * @brief This concept class describes the interface of a policy class
 *        for managing the queue(s) of a scheduler worker.
 */
class job_queue_policy {

 public:

    /**
     * @brief Enqueues a new job to the worker's queue from an
     *        external source, i.e., from any other thread.
     */
    template<class Worker>
    void external_enqueue(Worker* self, resumable* job);

    /**
     * @brief Enqueues a new job to the worker's queue from an
     *        internal source, i.e., from the same thread.
     */
    template<class Worker>
    void internal_enqueue(Worker* self, resumable* job);

    /**
     * @brief Called by external sources to try to dequeue an element.
     *        Returns @p nullptr if no element could be dequeued immediately.
     */
    template<class Worker>
    resumable* try_external_dequeue(Worker* self);

    /**
     * @brief Called by the worker itself to acquire a new job.
     *        Blocks until a job could be dequeued.
     */
    template<class Worker>
    resumable* internal_dequeue(Worker* self);

    /**
     * @brief Moves all elements form the internal queue to the external queue.
     */
    template<class Worker>
    void clear_internal_queue(Worker* self);

    /**
     * @brief Tries to move at least one element from the internal queue ot
     *        the external queue if possible to allow others to steal from us.
     */
    template<class Worker>
    void assert_stealable(Worker* self);

    /**
     * @brief Applies given functor to all elements in all queues and
     *        clears all queues afterwards.
     */
    template<class Worker, typename UnaryFunction>
    void consume_all(Worker* self, UnaryFunction f);

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_JOB_QUEUE_POLICY_HPP
