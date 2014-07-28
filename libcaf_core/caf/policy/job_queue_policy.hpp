/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_POLICY_JOB_QUEUE_POLICY_HPP
#define CAF_POLICY_JOB_QUEUE_POLICY_HPP

#include "caf/fwd.hpp"

namespace caf {
namespace policy {

/**
 * This concept class describes the interface of a policy class
 * for managing the queue(s) of a scheduler worker.
 */
class job_queue_policy {

 public:

  /**
   * Enqueues a new job to the worker's queue from an
   * external source, i.e., from any other thread.
   */
  template <class Worker>
  void external_enqueue(Worker* self, resumable* job);

  /**
   * Enqueues a new job to the worker's queue from an
   * internal source, i.e., from the same thread.
   */
  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job);

  /**
   * Returns `nullptr` if no element could be dequeued immediately.
   * Called by external sources to try to dequeue an element.
   */
  template <class Worker>
  resumable* try_external_dequeue(Worker* self);

  /**
   * Blocks until a job could be dequeued.
   * Called by the worker itself to acquire a new job.
   */
  template <class Worker>
  resumable* internal_dequeue(Worker* self);

  /**
   * Moves all elements form the internal queue to the external queue.
   */
  template <class Worker>
  void clear_internal_queue(Worker* self);

  /**
   * Tries to move at least one element from the internal queue to
   * the external queue if possible to allow others to steal from us.
   */
  template <class Worker>
  void assert_stealable(Worker* self);

  /**
   * Applies given functor to all elements in all queues and
   * clears all queues afterwards.
   */
  template <class Worker, typename UnaryFunction>
  void consume_all(Worker* self, UnaryFunction f);

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_JOB_QUEUE_POLICY_HPP
