// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>
#include <memory>

namespace caf {

/// A scheduler is responsible for managing the execution of resumables.
class CAF_CORE_EXPORT scheduler {
public:
  // -- factory functions ------------------------------------------------------

  static std::unique_ptr<scheduler> make_work_stealing(actor_system& sys);

  static std::unique_ptr<scheduler> make_work_sharing(actor_system& sys);

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~scheduler();

  /// Schedules @p what to run at some point in the future.
  /// @threadsafe
  virtual void schedule(resumable* what, uint64_t event_id) = 0;

  /// Delay the next execution of @p what. Unlike `schedule`, this function is
  /// not thread-safe and must be called only from the scheduler thread that is
  /// currently running.
  virtual void delay(resumable* what, uint64_t event_id) = 0;

  /// Returns `true` if this scheduler is part of the default system scheduler.
  virtual bool is_system_scheduler() const noexcept = 0;

  /// Starts this scheduler and all of its workers.
  virtual void start() = 0;

  /// Stops this scheduler and all of its workers.
  virtual void stop() = 0;
};

} // namespace caf
