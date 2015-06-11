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
namespace exit_reason {

/// Indicates that the actor is still alive.
static constexpr uint32_t not_exited = 0x00000;

/// Indicates that an actor finished execution.
static constexpr uint32_t normal = 0x00001;

/// Indicates that an actor finished execution because of an unhandled exception.
static constexpr uint32_t unhandled_exception = 0x00002;

/// Indicates that the actor received an unexpected synchronous reply message.
static constexpr uint32_t unhandled_sync_failure = 0x00004;

/// Indicates that the exit reason for this actor is unknown, i.e.,
/// the actor has been terminated and no longer exists.
static constexpr uint32_t unknown = 0x00006;

/// Indicates that an actor pool unexpectedly ran out of workers.
static constexpr uint32_t out_of_workers = 0x00007;

/// Indicates that the actor was forced to shutdown by a user-generated event.
static constexpr uint32_t user_shutdown = 0x00010;

/// Indicates that the actor was killed unconditionally.
static constexpr uint32_t kill = 0x00011;

/// Indicates that an actor finishied execution because a connection
/// to a remote link was closed unexpectedly.
static constexpr uint32_t remote_link_unreachable = 0x00101;

/// Any user defined exit reason should have a value greater or
/// equal to prevent collisions with default defined exit reasons.
static constexpr uint32_t user_defined = 0x10000;

/// Returns a string representation of given exit reason.
const char* as_string(uint32_t value);

} // namespace exit_reason
} // namespace caf

#endif // CAF_EXIT_REASON_HPP
