// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/fwd.hpp"

namespace caf::flow::op {

/// Base class for operators that allow observers to pull items via
/// `request(n)`.
class CAF_CORE_EXPORT pullable {
public:
  pullable();

  pullable(const pullable&) = delete;

  pullable& operator=(const pullable&) = delete;

  virtual ~pullable();

  /// Checks whether this operator is currently running `do_pull` or is
  /// scheduled to do so.
  bool is_pulling() const noexcept {
    return in_flight_demand_ > 0;
  }

protected:
  void pull(flow::coordinator* parent, size_t n);

private:
  virtual void do_pull(size_t in_flight_demand) = 0;

  /// Increments the reference count of this object.
  virtual void do_ref() = 0;

  /// Decrements the reference count of this object.
  virtual void do_deref() = 0;

  /// Stores how much demand is currently in flight. When this counter becomes
  /// non-zero, we schedule a call to do_pull via pull_cb_.
  size_t in_flight_demand_ = 0;

  /// The action for scheduling calls to `do_pull`. We re-use this action to
  /// avoid frequent allocations.
  action pull_cb_;
};

} // namespace caf::flow::op
