// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/async/fwd.hpp"
#include "caf/cow_string.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caf::flow {

/// Coordinates any number of co-located observables and observers. The
/// co-located objects never need to synchronize calls to other co-located
/// objects since the coordinator guarantees synchronous execution.
class CAF_CORE_EXPORT coordinator : public async::execution_context {
public:
  // -- friends ----------------------------------------------------------------

  template <class>
  friend class observable;

  // -- member types -----------------------------------------------------------

  /// A time point of the monotonic clock.
  using steady_time_point = std::chrono::steady_clock::time_point;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~coordinator();

  // -- factories --------------------------------------------------------------

  /// Returns a factory object for new observable objects on this coordinator.
  [[nodiscard]] observable_builder make_observable();

  /// Creates a new @ref coordinated object on this coordinator.
  template <class Impl, class... Args>
  [[nodiscard]] std::enable_if_t<std::is_base_of_v<coordinated, Impl>,
                                 intrusive_ptr<Impl>>
  add_child(std::in_place_type_t<Impl>, Args&&... args) {
    return make_counted<Impl>(this, std::forward<Args>(args)...);
  }

  /// Like @ref add_child, but wraps the result in a handle type. The handle
  /// type depends on the @ref coordinated object and usually one of
  /// `observer<T>`, `observable<T>`, or `subscription`.
  template <class Impl, class... Args>
  [[nodiscard]] std::enable_if_t<std::is_base_of_v<coordinated, Impl>,
                                 typename Impl::handle_type>
  add_child_hdl(std::in_place_type_t<Impl> token, Args&&... args) {
    using handle_type = typename Impl::handle_type;
    return handle_type{add_child(token, std::forward<Args>(args)...)};
  }

  // -- lifetime management ----------------------------------------------------

  /// Resets `child` and releases the reference count of the @ref coordinated
  /// object at the end of the current cycle.
  /// @post `child == nullptr`
  virtual void release_later(coordinated_ptr& child) = 0;

  /// Resets `child` and releases the reference count of the @ref coordinated
  /// object at the end of the current cycle.
  /// @post `child == nullptr`
  template <class T>
  std::enable_if_t<std::is_base_of_v<coordinated, T>>
  release_later(intrusive_ptr<T>& child) {
    auto ptr = coordinated_ptr{child.release(), false};
    release_later(ptr);
  }

  /// Resets `hdl` and releases the reference count of the @ref coordinated
  /// object at the end of the current cycle.
  /// @post `child == nullptr`
  template <class Handle>
  std::enable_if<Handle::holds_coordinated> release_later(Handle& hdl) {
    auto ptr = coordinated_ptr{hdl.as_intrusive_ptr().release(), false};
    release_later(ptr);
  }

  // -- time -------------------------------------------------------------------

  /// Returns the current time on the monotonic clock of this coordinator.
  virtual steady_time_point steady_time() = 0;

  // -- scheduling of actions --------------------------------------------------

  /// Delays execution of an action until all pending actions were executed. May
  /// call `schedule`.
  /// @param what The action for delayed execution.
  virtual void delay(action what) = 0;

  ///@copydoc delay
  template <class F>
  void delay_fn(F&& what) {
    return delay(make_single_shot_action(std::forward<F>(what)));
  }

  /// Delays execution of an action with an absolute timeout.
  /// @param abs_time The absolute time when this action should take place.
  /// @param what The action for delayed execution.
  /// @returns a @ref disposable to cancel the pending timeout.
  virtual disposable delay_until(steady_time_point abs_time, action what) = 0;

  ///@copydoc delay
  template <class F>
  disposable delay_until_fn(steady_time_point abs_time, F&& what) {
    return delay_until(abs_time,
                       make_single_shot_action(std::forward<F>(what)));
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
  disposable delay_for_fn(timespan rel_time, F&& what) {
    return delay_for(rel_time, make_single_shot_action(std::forward<F>(what)));
  }

private:
  virtual stream
  to_stream_impl(cow_string name,
                 intrusive_ptr<flow::op::base<async::batch>> batch_op,
                 type_id_t item_type, size_t max_items_per_batch);
};

/// @relates coordinator
using coordinator_ptr = intrusive_ptr<coordinator>;

} // namespace caf::flow
