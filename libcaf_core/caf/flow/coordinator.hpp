// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>

#include "caf/action.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

namespace caf::flow {

/// Coordinates any number of co-located publishers and observers. The
/// co-located objects never need to synchronize calls to other co-located
/// objects since the coordinator guarantees synchronous execution.
class CAF_CORE_EXPORT coordinator {
public:
  // class CAF_CORE_EXPORT subscription_impl : public subscription::impl {
  // public:
  //   friend class coordinator;

  //   subscription_impl(coordinator* ctx, observable_base_ptr src,
  //                     observer_base_ptr snk)
  //     : ctx_(ctx), src_(std::move(src)), snk_(std::move(snk)) {
  //     // nop
  //   }

  //   void request(size_t n) final;

  //   void cancel() final;

  //   bool disposed() const noexcept final;

  //   auto* ctx() const noexcept {
  //     return ctx_;
  //   }

  // private:
  //   coordinator* ctx_;
  //   observable_base_ptr src_;
  //   observer_base_ptr snk_;
  // };

  // using subscription_impl_ptr = intrusive_ptr<subscription_impl>;

  friend class subscription_impl;

  virtual ~coordinator();

  [[nodiscard]] observable_builder make_observable();

  /// Increases the reference count of the coordinator.
  virtual void ref_coordinator() const noexcept = 0;

  /// Decreases the reference count of the coordinator and destroys the object
  /// if necessary.
  virtual void deref_coordinator() const noexcept = 0;

  /// Schedules an action for execution on this coordinator. This member
  /// function may get called from external sources or threads.
  /// @thread-safe
  virtual void schedule(action what) = 0;

  ///@copydoc schedule
  template <class F>
  void schedule_fn(F&& what) {
    return schedule(make_action(std::forward<F>(what)));
  }

  /// Schedules an action for execution from within the coordinator. May call
  /// `schedule` for coordinators that use a single work queue.
  virtual void post_internally(action what) = 0;

  ///@copydoc schedule
  template <class F>
  void post_internally_fn(F&& what) {
    return post_internally(make_action(std::forward<F>(what)));
  }

  /// Asks the coordinator to keep its event loop running until `obj` becomes
  /// disposed since it depends on external events or produces events that are
  /// visible to outside observers.
  virtual void watch(disposable what) = 0;

  friend void intrusive_ptr_add_ref(const coordinator* ptr) noexcept {
    ptr->ref_coordinator();
  }

  friend void intrusive_ptr_release(const coordinator* ptr) noexcept {
    ptr->deref_coordinator();
  }
};

/// @relates coordinator
using coordinator_ptr = intrusive_ptr<coordinator>;

} // namespace caf::flow
