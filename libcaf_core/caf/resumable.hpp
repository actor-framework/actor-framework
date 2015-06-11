/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_RESUMABLE_HPP
#define CAF_RESUMABLE_HPP

#include <cstddef> // size_t

namespace caf {

class execution_unit;

/// A cooperatively executed task managed by one or more
/// instances of `execution_unit`.
class resumable {
public:
  enum resume_result {
    resume_later,
    awaiting_message,
    done,
    shutdown_execution_unit
  };

  resumable() = default;

  virtual ~resumable();

  /// Initializes this object, e.g., by increasing the the reference count.
  virtual void attach_to_scheduler() = 0;

  /// Uninitializes this object, e.g., by decrementing the the reference count.
  virtual void detach_from_scheduler() = 0;

  /// Resume any pending computation until it is either finished
  /// or needs to be re-scheduled later.
  virtual resume_result resume(execution_unit*, size_t max_throughput) = 0;
};

} // namespace caf

#endif // CAF_RESUMABLE_HPP
