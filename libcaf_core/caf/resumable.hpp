// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <type_traits>

namespace caf {

/// A cooperatively executed task managed by one or more
/// instances of `execution_unit`. Note that this class is
/// meant as mixin for reference counted object, i.e., the
/// subclass is required to inherit from `ref_counted`
/// at some point.
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

  resumable() = default;

  virtual ~resumable();

  /// Returns a subtype hint for this object. This allows an execution
  /// unit to limit processing to a specific set of resumables and
  /// delegate other subtypes to dedicated workers.
  virtual subtype_t subtype() const;

  /// Resume any pending computation until it is either finished
  /// or needs to be re-scheduled later.
  virtual resume_result resume(execution_unit*, size_t max_throughput) = 0;

  /// Add a strong reference count to this object.
  virtual void intrusive_ptr_add_ref_impl() = 0;

  /// Remove a strong reference count from this object.
  virtual void intrusive_ptr_release_impl() = 0;
};

// enables intrusive_ptr<resumable> without introducing ambiguity
template <class T>
std::enable_if_t<std::is_same_v<T*, resumable*>> intrusive_ptr_add_ref(T* ptr) {
  ptr->intrusive_ptr_add_ref_impl();
}

// enables intrusive_ptr<resumable> without introducing ambiguity
template <class T>
std::enable_if_t<std::is_same_v<T*, resumable*>> intrusive_ptr_release(T* ptr) {
  ptr->intrusive_ptr_release_impl();
}

} // namespace caf
