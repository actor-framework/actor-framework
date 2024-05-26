// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/flow/coordinated.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/subscription.hpp"

namespace caf::flow::op {

/// Abstract base type for all flow operators that implement the *observable*
/// concept.
template <class T>
class base : public coordinated {
public:
  // -- member types -----------------------------------------------------------

  /// The derived type.
  using super = coordinated;

  /// The type of observed values.
  using output_type = T;

  /// The proper type for holding a type-erased handle to object instances.
  using handle_type = observable<T>;

  /// Subscribes a new observer to the operator.
  virtual disposable subscribe(observer<T> what) = 0;

  /// Calls `on_subscribe` and `on_error` on `out` to immediately fail a
  /// subscription.
  /// @relates observer
  disposable fail_subscription(observer<T>& out, const error& err) {
    using sub_t = subscription::trivial_impl;
    auto sub = parent()->add_child(std::in_place_type<sub_t>);
    out.on_subscribe(subscription{sub});
    if (!sub->disposed())
      out.on_error(err);
    return sub->as_disposable();
  }

  /// Calls `on_subscribe` and `on_complete` on `out` to immediately complete a
  /// subscription.
  /// @relates observer
  disposable empty_subscription(observer<T>& out) {
    using sub_t = subscription::trivial_impl;
    auto sub = parent()->add_child(std::in_place_type<sub_t>);
    out.on_subscribe(subscription{sub});
    if (!sub->disposed())
      out.on_complete();
    return sub->as_disposable();
  }
};

} // namespace caf::flow::op
