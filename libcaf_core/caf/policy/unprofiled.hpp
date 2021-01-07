// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf::policy {

/// This class is intended to be used as a base class for actual polices.
/// It provides a default empty implementation for the customization points.
/// By deriving from it, actual policy classes only need to implement/override
/// the customization points they need. This class also serves as a place to
/// factor common utilities for implementing actual policies.
class CAF_CORE_EXPORT unprofiled {
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

} // namespace caf::policy
