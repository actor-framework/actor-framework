// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <chrono>
#include <string>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// A monotonic clock for scheduling timeouts and delayed messages.
class CAF_CORE_EXPORT actor_clock {
public:
  // -- member types -----------------------------------------------------------

  /// Underlying clock type.
  using clock_type = std::chrono::steady_clock;

  /// Discrete point in time.
  using time_point = typename clock_type::time_point;

  /// Time interval.
  using duration_type = typename clock_type::duration;

  /// Configures how the clock responds to a stalling actor when trying to
  /// schedule a periodic action.
  enum class stall_policy {
    fail, /// Causes the clock to dispose an action send an error to the actor.
    skip, /// Causes the clock to skip scheduled runs without emitting errors.
  };

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~actor_clock();

  // -- scheduling -------------------------------------------------------------

  /// Returns the current wall-clock time.
  virtual time_point now() const noexcept;

  /// Schedules an action for execution.
  /// @param f The action to schedule.
  /// @note The action runs on the thread of the clock worker and thus must
  ///       complete within a very short time in order to not delay other work.
  disposable schedule(action f);

  /// Schedules an action for execution at a later time.
  /// @param t The local time at which the action should run.
  /// @param f The action to schedule.
  /// @note The action runs on the thread of the clock worker and thus must
  ///       complete within a very short time in order to not delay other work.
  disposable schedule(time_point t, action f);

  /// Schedules an action for periodic execution.
  /// @param first_run The local time at which the action should run initially.
  /// @param f The action to schedule.
  /// @param period The time to wait between two runs. A non-positive period
  ///               disables periodic execution.
  /// @note The action runs on the thread of the clock worker and thus must
  ///       complete within a very short time in order to not delay other work.
  virtual disposable
  schedule_periodically(time_point first_run, action f, duration_type period)
    = 0;

  /// Schedules an action for execution by an actor at a later time.
  /// @param t The local time at which the action should get enqueued to the
  ///          mailbox of the target.
  /// @param f The action to schedule.
  /// @param target The actor that should run the action.
  disposable schedule(time_point t, action f, strong_actor_ptr target);

  /// Schedules an action for periodic execution by an actor.
  /// @param first_run The local time at which the action should get enqueued to
  ///                  the mailbox of the target for the first time.
  /// @param f The action to schedule.
  /// @param target The actor that should run the action.
  /// @param period The time to wait between two messages to the actor.
  /// @param policy The strategy for dealing with a stalling target.
  disposable schedule_periodically(time_point first_run, action f,
                                   strong_actor_ptr target,
                                   duration_type period, stall_policy policy);

  /// Schedules an action for execution by an actor at a later time.
  /// @param target The actor that should run the action.
  /// @param f The action to schedule.
  /// @param t The local time at which the action should get enqueued to the
  ///          mailbox of the target.
  disposable schedule(time_point t, action f, weak_actor_ptr target);

  /// Schedules an action for periodic execution by an actor.
  /// @param first_run The local time at which the action should get enqueued to
  ///                  the mailbox of the target for the first time.
  /// @param f The action to schedule.
  /// @param target The actor that should run the action.
  /// @param period The time to wait between two messages to the actor.
  /// @param policy The strategy for dealing with a stalling target.
  disposable schedule_periodically(time_point first_run, action f,
                                   weak_actor_ptr target, duration_type period,
                                   stall_policy policy);

  /// Schedules an arbitrary message to `receiver` for time point `t`.
  disposable schedule_message(time_point t, strong_actor_ptr receiver,
                              mailbox_element_ptr content);

  /// Schedules an arbitrary message to `receiver` for time point `t`.
  disposable schedule_message(time_point t, weak_actor_ptr receiver,
                              mailbox_element_ptr content);

  /// Schedules an arbitrary message to `target` for time point `t`.
  disposable schedule_message(time_point t, group target,
                              strong_actor_ptr sender, message content);
};

} // namespace caf
