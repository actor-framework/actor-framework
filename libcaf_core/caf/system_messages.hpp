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

#ifndef CAF_SYSTEM_MESSAGES_HPP
#define CAF_SYSTEM_MESSAGES_HPP

#include <vector>
#include <cstdint>
#include <type_traits>

#include "caf/group.hpp"
#include "caf/actor_addr.hpp"

#include "caf/detail/tbind.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {

/// Sent to all links when an actor is terminated.
/// @note This message can be handled manually by calling
///       `local_actor::trap_exit(true)` and is otherwise handled
///       implicitly by the runtime system.
struct exit_msg {
  /// The source of this message, i.e., the terminated actor.
  actor_addr source;
  /// The exit reason of the terminated actor.
  uint32_t reason;
};

/// Sent to all actors monitoring an actor when it is terminated.
struct down_msg {
  /// The source of this message, i.e., the terminated actor.
  actor_addr source;
  /// The exit reason of the terminated actor.
  uint32_t reason;
};

/// Sent whenever a terminated actor receives a synchronous request.
struct sync_exited_msg {
  /// The source of this message, i.e., the terminated actor.
  actor_addr source;
  /// The exit reason of the terminated actor.
  uint32_t reason;
};

template <class T>
typename std::enable_if<
  detail::tl_exists<detail::type_list<exit_msg, down_msg, sync_exited_msg>,
            detail::tbind<std::is_same, T>::template type>::value,
  bool>::type
operator==(const T& lhs, const T& rhs) {
  return lhs.source == rhs.source && lhs.reason == rhs.reason;
}

template <class T>
typename std::enable_if<
  detail::tl_exists<detail::type_list<exit_msg, down_msg, sync_exited_msg>,
            detail::tbind<std::is_same, T>::template type>::value,
  bool>::type
operator!=(const T& lhs, const T& rhs) {
  return !(lhs == rhs);
}

/// Sent to all members of a group when it goes offline.
struct group_down_msg {
  /// The source of this message, i.e., the now unreachable group.
  group source;
};

inline bool operator==(const group_down_msg& lhs, const group_down_msg& rhs) {
  return lhs.source == rhs.source;
}

inline bool operator!=(const group_down_msg& lhs, const group_down_msg& rhs) {
  return !(lhs == rhs);
}

/// Sent whenever a timeout occurs during a synchronous send.
/// This system message does not have any fields, because the message ID
/// sent alongside this message identifies the matching request that timed out.
struct sync_timeout_msg { };

/// @relates exit_msg
inline bool operator==(const sync_timeout_msg&, const sync_timeout_msg&) {
  return true;
}

/// @relates exit_msg
inline bool operator!=(const sync_timeout_msg&, const sync_timeout_msg&) {
  return false;
}

/// Signalizes a timeout event.
/// @note This message is handled implicitly by the runtime system.
struct timeout_msg {
  /// Actor-specific timeout ID.
  uint32_t timeout_id;
};

inline bool operator==(const timeout_msg& lhs, const timeout_msg& rhs) {
  return lhs.timeout_id == rhs.timeout_id;
}

inline bool operator!=(const timeout_msg& lhs, const timeout_msg& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

#endif // CAF_SYSTEM_MESSAGES_HPP
