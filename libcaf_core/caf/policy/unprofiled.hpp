/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_POLICY_UNPROFILED_HPP
#define CAF_POLICY_UNPROFILED_HPP

#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {
namespace policy {

/// This class is intended to be used as a base class for actual polices.
/// It provides a default empty implementation for the customization points.
/// By deriving from it, actual policy classes only need to implement/override
/// the customization points they need. This class also serves as a place to
/// factor common utilities for implementing actual policies.
class unprofiled {
public:
  virtual ~unprofiled();

  /// Performs cleanup action before a shutdown takes place.
  template <class Worker>
  void before_shutdown(Worker*) {
    // nop
  }

  /// Called immediately before resuming an actor.
  template <class Worker>
  void before_resume(Worker*, resumable*) {
    // nop
  }

  /// Called whenever an actor has been resumed. This function can
  /// prepare some fields before the next resume operation takes place
  /// or perform cleanup actions between to actor runs.
  template <class Worker>
  void after_resume(Worker*, resumable*) {
    // nop
  }

  /// Called whenever an actor has completed a job.
  template <class Worker>
  void after_completion(Worker*, resumable*) {
    // nop
  }

protected:
  // Convenience function to access the data field.
  template <class WorkerOrCoordinator>
  static auto d(WorkerOrCoordinator* self) -> decltype(self->data()) {
    return self->data();
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_UNPROFILED_HPP
