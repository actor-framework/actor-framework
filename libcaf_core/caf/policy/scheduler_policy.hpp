/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_POLICY_SCHEDULER_POLICY_HPP
#define CAF_POLICY_SCHEDULER_POLICY_HPP

#include "caf/fwd.hpp"

namespace caf {
namespace policy {

/// This concept class describes a policy for worker
/// and coordinator of the scheduler.
class scheduler_policy {
public:
  /// Policy-specific data fields for the coordinator.
  struct coordinator_data { };

  /// Policy-specific data fields for the worker.
  struct worker_data { };

  /// Enqueues a new job to coordinator.
  template <class Coordinator>
  void central_enqueue(Coordinator* self, resumable* job);

  /// Enqueues a new job to the worker's queue from an
  /// external source, i.e., from any other thread.
  template <class Worker>
  void external_enqueue(Worker* self, resumable* job);

  /// Enqueues a new job to the worker's queue from an
  /// internal source, i.e., from the same thread.
  template <class Worker>
  void internal_enqueue(Worker* self, resumable* job);

  /// Called whenever resumable returned for reason `resumable::resume_later`.
  template <class Worker>
  void resume_job_later(Worker* self, resumable* job);

  /// Blocks until a job could be dequeued.
  /// Called by the worker itself to acquire a new job.
  template <class Worker>
  resumable* dequeue(Worker* self);

  /// Performs cleanup action before a shutdown takes place.
  template <class Worker>
  void before_shutdown(Worker* self);

  /// Called immediately before resuming an actor.
  template <class Worker>
  void before_resume(Worker* self, resumable* job);

  /// Called whenever an actor has been resumed. This function can
  /// prepare some fields before the next resume operation takes place
  /// or perform cleanup actions between to actor runs.
  template <class Worker>
  void after_resume(Worker* self, resumable* job);

  /// Called whenever an actor has completed a job.
  template <class Worker>
  void after_completion(Worker* self, resumable* job);

  /// Applies given functor to all resumables attached to a worker.
  template <class Worker, typename UnaryFunction>
  void foreach_resumable(Worker* self, UnaryFunction f);

  /// Applies given functor to all resumables attached to the coordinator.
  template <class Coordinator, typename UnaryFunction>
  void foreach_central_resumable(Coordinator* self, UnaryFunction f);
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_SCHEDULER_POLICY_HPP
