// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <concepts>
#include <type_traits>

namespace caf {

/// A cooperatively scheduled entity.
class CAF_CORE_EXPORT resumable {
public:
  /// Denotes the state in which a `resumable`
  /// returned from its last call to `resume`.
  enum resume_result {
    resume_later,
    awaiting_message,
    done,
    shutdown_execution_unit
  };

  /// Denotes common subtypes of `resumable`.
  enum subtype_t {
    /// Identifies non-actors or blocking actors.
    unspecified,
    /// Identifies event-based, cooperatively scheduled actors.
    scheduled_actor,
    /// Identifies broker, i.e., actors performing I/O.
    io_actor,
    /// Identifies tasks, usually one-shot callbacks.
    function_object
  };

  /// The event ID for disposing a resumable.
  static constexpr uint64_t dispose_event_id = 0;

  /// The event ID for the default event.
  static constexpr uint64_t initialization_event_id = 1;

  /// The event ID for the default event.
  static constexpr uint64_t default_event_id = 2;

  resumable() = default;

  virtual ~resumable();

  /// Returns a subtype hint for this object. This allows an execution
  /// unit to limit processing to a specific set of resumables and
  /// delegate other subtypes to dedicated workers.
  virtual subtype_t subtype() const noexcept;

  /// Resume any pending computation until it is either finished
  /// or needs to be re-scheduled later.
  /// @param event_id The event ID for the event that triggered the resume.
  /// @returns The result of the resume.
  virtual resume_result resume(scheduler*, uint64_t event_id) = 0;

  /// Add a strong reference count to this object.
  virtual void ref_resumable() const noexcept = 0;

  /// Remove a strong reference count from this object.
  virtual void deref_resumable() const noexcept = 0;
};

// enables intrusive_ptr<resumable> without introducing ambiguity
template <std::same_as<resumable> T>
void intrusive_ptr_add_ref(const T* ptr) noexcept {
  ptr->ref_resumable();
}

// enables intrusive_ptr<resumable> without introducing ambiguity
template <std::same_as<resumable> T>
void intrusive_ptr_release(const T* ptr) noexcept {
  ptr->deref_resumable();
}

} // namespace caf
