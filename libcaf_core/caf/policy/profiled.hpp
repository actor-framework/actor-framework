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

#ifndef CAF_POLICY_PROFILED_HPP
#define CAF_POLICY_PROFILED_HPP

#include "caf/resumable.hpp"
#include "caf/abstract_actor.hpp"

namespace caf {

namespace scheduler  {

template <class>
class profiled_coordinator;

} // namespace scheduler

namespace policy {

/// An enhancement of CAF's scheduling policy which records fine-grained
/// resource utiliziation for worker threads and actors in the parent
/// coordinator of the workers.
template <class Policy>
struct profiled : Policy {
  using coordinator_type = scheduler::profiled_coordinator<profiled<Policy>>;

  static actor_id id_of(resumable* job) {
    auto ptr = dynamic_cast<abstract_actor*>(job);
    return ptr ? ptr->id() : 0;
  }

  template <class Worker>
  void before_resume(Worker* worker, resumable* job) {
    Policy::before_resume(worker, job);
    auto parent = static_cast<coordinator_type*>(worker->parent());
    parent->start_measuring(worker->id(), id_of(job));
  }

  template <class Worker>
  void after_resume(Worker* worker, resumable* job) {
    Policy::after_resume(worker, job);
    auto parent = static_cast<coordinator_type*>(worker->parent());
    parent->stop_measuring(worker->id(), id_of(job));
  }

  template <class Worker>
  void after_completion(Worker* worker, resumable* job) {
    Policy::after_completion(worker, job);
    auto parent = static_cast<coordinator_type*>(worker->parent());
    parent->remove_job(id_of(job));
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_PROFILED_HPP
