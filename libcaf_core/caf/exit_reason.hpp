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

#ifndef CAF_EXIT_REASON_HPP
#define CAF_EXIT_REASON_HPP

#include <cstdint>

namespace caf {

enum class exit_reason : uint8_t {
  /// Indicates that the actor is still alive.
  not_exited = 0x00,

  /// Indicates that an actor finished execution.
  normal = 0x01,

  /// Indicates that an actor finished execution because of an unhandled exception.
  unhandled_exception = 0x02,

  /// Indicates that the actor received an unexpected synchronous reply message.
  unhandled_sync_failure = 0x04,

  /// Indicates that the exit reason for this actor is unknown, i.e.,
  /// the actor has been terminated and no longer exists.
  unknown = 0x06,

  /// Indicates that an actor pool unexpectedly ran out of workers.
  out_of_workers = 0x07,

  /// Indicates that the actor was forced to shutdown by a user-generated event.
  user_shutdown = 0x10,

  /// Indicates that the actor was killed unconditionally.
  kill = 0x11,

  /// Indicates that an actor finishied execution because a connection
  /// to a remote link was closed unexpectedly.
  remote_link_unreachable = 0x21,

  /// Indicates that the actor was killed because it became unreachable.
  unreachable = 0x40
};

/// Returns a string representation of given exit reason.
const char* to_string(exit_reason x);

} // namespace caf

#endif // CAF_EXIT_REASON_HPP
