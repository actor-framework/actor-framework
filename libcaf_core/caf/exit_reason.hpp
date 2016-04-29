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

#include "caf/error.hpp"

namespace caf {

enum class exit_reason : uint8_t {
  /// Indicates that an actor finished execution without error.
  normal = 0x00,

  /// Indicates that an actor finished execution because of an unhandled exception.
  unhandled_exception = 0x02,

  /// Indicates that the actor received an unexpected synchronous reply message.
  unhandled_request_error = 0x04,

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
  unreachable = 0x40,

  /// Indicates that the actor was killed after receiving an error message.
  unhandled_error = 0x41
};

/// Returns a string representation of given exit reason.
const char* to_string(exit_reason x);

/// @relates exit_reason
error make_error(exit_reason);

/// @relates exit_reason
error make_error(exit_reason, message context);

} // namespace caf

#endif // CAF_EXIT_REASON_HPP
