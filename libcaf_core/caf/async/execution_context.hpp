// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_ref_counted.hpp"
#include "caf/action.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"

#include <type_traits>

namespace caf::async {

/// Represents a single execution context with an internal event-loop to
/// schedule @ref action objects.
class CAF_CORE_EXPORT execution_context : public abstract_ref_counted {
public:
  // -- constructors, destructors, and assignment operators --------------------

  virtual ~execution_context();

  // -- scheduling of actions --------------------------------------------------

  /// Schedules @p what to run on the event loop of the execution context. This
  /// member function may get called from external sources or threads.
  /// @threadsafe
  virtual void schedule(action what) = 0;

  ///@copydoc schedule
  template <class F>
  void schedule_fn(F&& what) {
    static_assert(std::is_invocable_v<F>);
    return schedule(make_single_shot_action(std::forward<F>(what)));
  }

  // -- lifetime management ----------------------------------------------------

  /// Asks the coordinator to keep its event loop running until @p what becomes
  /// disposed since it depends on external events or produces events that are
  /// visible to outside observers. Must be called from within the event loop of
  /// the execution context.
  virtual void watch(disposable what) = 0;
};

/// @relates execution_context
using execution_context_ptr = intrusive_ptr<execution_context>;

} // namespace caf::async
