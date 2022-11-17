// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"

#include <type_traits>

namespace caf::async {

/// Represents a single execution context with an internal event-loop to
/// schedule @ref action objects.
class CAF_CORE_EXPORT execution_context {
public:
  // -- constructors, destructors, and assignment operators --------------------

  virtual ~execution_context();

  // -- reference counting -----------------------------------------------------

  /// Increases the reference count of the execution_context.
  virtual void ref_execution_context() const noexcept = 0;

  /// Decreases the reference count of the execution context and destroys the
  /// object if necessary.
  virtual void deref_execution_context() const noexcept = 0;

  // -- scheduling of actions --------------------------------------------------

  /// Schedules @p what to run on the event loop of the execution context. This
  /// member function may get called from external sources or threads.
  /// @thread-safe
  virtual void schedule(action what) = 0;

  ///@copydoc schedule
  template <class F>
  void schedule_fn(F&& what) {
    static_assert(std::is_invocable_v<F>);
    return schedule(make_action(std::forward<F>(what)));
  }

  // -- lifetime management ----------------------------------------------------

  /// Asks the coordinator to keep its event loop running until @p what becomes
  /// disposed since it depends on external events or produces events that are
  /// visible to outside observers. Must be called from within the event loop of
  /// the execution context.
  virtual void watch(disposable what) = 0;
};

/// @relates execution_context
inline void intrusive_ptr_add_ref(const execution_context* ptr) noexcept {
  ptr->ref_execution_context();
}

/// @relates execution_context
inline void intrusive_ptr_release(const execution_context* ptr) noexcept {
  ptr->deref_execution_context();
}

/// @relates execution_context
using execution_context_ptr = intrusive_ptr<execution_context>;

} // namespace caf::async
