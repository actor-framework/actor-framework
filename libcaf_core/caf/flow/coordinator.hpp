// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <tuple>

namespace caf::flow {

/// Coordinates any number of co-located observables and observers. The
/// co-located objects never need to synchronize calls to other co-located
/// objects since the coordinator guarantees synchronous execution.
class CAF_CORE_EXPORT coordinator {
public:
  // -- friends ----------------------------------------------------------------

  friend class subscription_impl;

  // -- member types -----------------------------------------------------------

  /// A time point of the monotonic clock.
  using steady_time_point = std::chrono::steady_clock::time_point;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~coordinator();

  // -- reference counting -----------------------------------------------------

  /// Increases the reference count of the coordinator.
  virtual void ref_coordinator() const noexcept = 0;

  /// Decreases the reference count of the coordinator and destroys the object
  /// if necessary.
  virtual void deref_coordinator() const noexcept = 0;

  friend void intrusive_ptr_add_ref(const coordinator* ptr) noexcept {
    ptr->ref_coordinator();
  }

  friend void intrusive_ptr_release(const coordinator* ptr) noexcept {
    ptr->deref_coordinator();
  }

  // -- conversions ------------------------------------------------------------

  /// Returns a factory object for new observable objects on this coordinator.
  [[nodiscard]] observable_builder make_observable();

  // -- lifetime management ----------------------------------------------------

  /// Asks the coordinator to keep its event loop running until `obj` becomes
  /// disposed since it depends on external events or produces events that are
  /// visible to outside observers.
  virtual void watch(disposable what) = 0;

  // -- time -------------------------------------------------------------------

  /// Returns the current time on the monotonic clock of this coordinator.
  virtual steady_time_point steady_time() = 0;

  // -- scheduling of actions --------------------------------------------------

  /// Schedules an action for execution on this coordinator. This member
  /// function may get called from external sources or threads.
  /// @thread-safe
  virtual void schedule(action what) = 0;

  ///@copydoc schedule
  template <class F>
  void schedule_fn(F&& what) {
    return schedule(make_action(std::forward<F>(what)));
  }

  /// Delays execution of an action until all pending actions were executed. May
  /// call `schedule`.
  /// @param what The action for delayed execution.
  virtual void delay(action what) = 0;

  ///@copydoc delay
  template <class F>
  void delay_fn(F&& what) {
    return delay(make_action(std::forward<F>(what)));
  }

  /// Delays execution of an action with an absolute timeout.
  /// @param abs_time The absolute time when this action should take place.
  /// @param what The action for delayed execution.
  /// @returns a @ref disposable to cancel the pending timeout.
  virtual disposable delay_until(steady_time_point abs_time, action what) = 0;

  ///@copydoc delay
  template <class F>
  disposable delay_until_fn(steady_time_point abs_time, F&& what) {
    return delay_until(abs_time, make_action(std::forward<F>(what)));
  }

  /// Delays execution of an action with a relative timeout.
  /// @param rel_time The relative time when this action should take place.
  /// @param what The action for delayed execution.
  /// @returns a @ref disposable to cancel the pending timeout.
  disposable delay_for(timespan rel_time, action what) {
    return delay_until(steady_time() + rel_time, std::move(what));
  }

  ///@copydoc delay_for
  template <class F>
  void delay_for_fn(timespan rel_time, F&& what) {
    return delay_until(steady_time() + rel_time,
                       make_action(std::forward<F>(what)));
  }
};

/// @relates coordinator
using coordinator_ptr = intrusive_ptr<coordinator>;

} // namespace caf::flow
