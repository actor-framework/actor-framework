/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {

/// A cooperatively executed task managed by one or more
/// instances of `execution_unit`. Note that this class is
/// meant as mixin for reference counted object, i.e., the
/// subclass is required to inherit from `ref_counted`
/// at some point.
class resumable {
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
typename std::enable_if<
  std::is_same<T*, resumable*>::value
>::type
intrusive_ptr_add_ref(T* ptr) {
  ptr->intrusive_ptr_add_ref_impl();
}

// enables intrusive_ptr<resumable> without introducing ambiguity
template <class T>
typename std::enable_if<
  std::is_same<T*, resumable*>::value
>::type
intrusive_ptr_release(T* ptr) {
  ptr->intrusive_ptr_release_impl();
}

} // namespace caf

