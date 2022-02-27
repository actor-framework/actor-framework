// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <map>

#include "caf/action.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/core_export.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT test_actor_clock : public actor_clock {
public:
  // -- constructors, destructors, and assignment operators --------------------

  test_actor_clock();

  // -- overrides --------------------------------------------------------------

  time_point now() const noexcept override;

  disposable schedule(time_point abs_time, action f) override;

  // -- testing DSL API --------------------------------------------------------

  /// Returns whether the actor clock has at least one pending timeout.
  bool has_pending_timeout() const {
    auto not_disposed = [](const auto& kvp) { return !kvp.second.disposed(); };
    return std::any_of(actions.begin(), actions.end(), not_disposed);
  }

  /// Triggers the next pending timeout regardless of its timestamp. Sets
  /// `current_time` to the time point of the triggered timeout unless
  /// `current_time` is already set to a later time.
  /// @returns Whether a timeout was triggered.
  bool trigger_timeout();

  /// Triggers all pending timeouts regardless of their timestamp. Sets
  /// `current_time` to the time point of the latest timeout unless
  /// `current_time` is already set to a later time.
  /// @returns The number of triggered timeouts.
  size_t trigger_timeouts();

  /// Advances the time by `x` and dispatches timeouts and delayed messages.
  /// @returns The number of triggered timeouts.
  size_t advance_time(duration_type x);

  // -- properties -------------------------------------------------------------

  /// @pre has_pending_timeout()
  time_point next_timeout() const {
    return actions.begin()->first;
  }

  // -- member variables -------------------------------------------------------

  time_point current_time;

  std::multimap<time_point, action> actions;

private:
  bool try_trigger_once();
};

} // namespace caf::detail
