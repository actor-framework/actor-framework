// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Identifies an execution unit, e.g., a worker thread of the scheduler. By
/// querying its execution unit, an actor can access other context information.
class CAF_CORE_EXPORT execution_unit {
public:
  explicit execution_unit(actor_system* sys);

  execution_unit() = default;
  execution_unit(execution_unit&&) = default;
  execution_unit& operator=(execution_unit&&) = default;
  execution_unit(const execution_unit&) = default;
  execution_unit& operator=(const execution_unit&) = default;

  virtual ~execution_unit();

  /// Enqueues `ptr` to the job list of the execution unit.
  /// @warning Must only be called from a {@link resumable} currently
  ///          executed by this execution unit.
  virtual void exec_later(resumable* ptr) = 0;

  /// Returns the enclosing actor system.
  /// @warning Must be set before the execution unit calls `resume` on an actor.
  actor_system& system() const {
    CAF_ASSERT(system_ != nullptr);
    return *system_;
  }

protected:
  actor_system* system_ = nullptr;
};

} // namespace caf
