// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_ref_counted.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <concepts>

namespace caf {

/// A cooperatively scheduled entity.
class CAF_CORE_EXPORT resumable : public abstract_ref_counted {
public:
  /// The event ID for the default event.
  static constexpr uint64_t default_event_id = 0;

  /// The event ID for initialization events.
  static constexpr uint64_t initialization_event_id = 1;

  /// The event ID for disposing a resumable.
  static constexpr uint64_t dispose_event_id = 2;

  resumable() = default;

  virtual ~resumable() noexcept;

  /// Runs a pending event on the resumable.
  /// @param context The current scheduler that is running the resumable.
  /// @param event_id The event ID for the event that triggered the resume.
  /// @note Separate events are scheduled independently. If a resumable supports
  ///       multiple types of events, it needs to be prepared that `resume` may
  ///       be called concurrently.
  /// @note The default CAF scheduler does not support event IDs and always uses
  ///       the default event ID.
  virtual void resume(scheduler* context, uint64_t event_id) = 0;

  /// Returns the scheduler that this resumable is pinned to or `nullptr` if
  /// it can be scheduled on any scheduler.
  virtual scheduler* pinned_scheduler() const noexcept;
};

} // namespace caf
