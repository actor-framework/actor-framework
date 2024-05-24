// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_clock.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/actor_local_printer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

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
  /// @thread-safe
  virtual void schedule(resumable* what) = 0;

  /// Delay the next execution of @p what. Unlike `schedule`, this function is
  /// not thread-safe and must be called only from the scheduler thread that is
  /// currently running.
  virtual void delay(resumable* what) = 0;

  /// Starts this scheduler and all of its workers.
  virtual void start() = 0;

  /// Stops this scheduler and all of its workers.
  virtual void stop() = 0;
};

} // namespace caf
