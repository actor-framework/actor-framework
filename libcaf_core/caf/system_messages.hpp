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

#include "caf/fwd.hpp"
#include "caf/group.hpp"
#include "caf/actor_addr.hpp"
#include "caf/deep_to_string.hpp"

#include "caf/detail/tbind.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {

/// Sent to all links when an actor is terminated.
/// @note Actors can override the default handler by calling
///       `self->set_exit_handler(...)`.
class exit_msg {
public:
  // -- friend types that need access to private ctors -------------------------

  template <class>
  friend class data_processor;

  template <class>
  friend class detail::type_erased_value_impl;

  // -- constructors -----------------------------------------------------------

  inline exit_msg(actor_addr x, error y)
      : source(std::move(x)),
        reason(std::move(y)) {
    // nop
  }

  // -- data members -----------------------------------------------------------

  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;

private:
  exit_msg() = default;
};

inline std::string to_string(const exit_msg& x) {
  return "exit_msg" + deep_to_string(std::tie(x.source, x.reason));
}

template <class Processor>
void serialize(Processor& proc, exit_msg& x, const unsigned int) {
  proc & x.source;
  proc & x.reason;
}

/// Sent to all actors monitoring an actor when it is terminated.
class down_msg {
public:
  // -- friend types that need access to private ctors -------------------------

  template <class>
  friend class data_processor;

  template <class>
  friend class detail::type_erased_value_impl;

  // -- constructors -----------------------------------------------------------

  inline down_msg(actor_addr x, error y)
      : source(std::move(x)),
        reason(std::move(y)) {
    // nop
  }

  // -- data members -----------------------------------------------------------

  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;

private:
  down_msg() = default;
};

inline std::string to_string(const down_msg& x) {
  return "down_msg" + deep_to_string(std::tie(x.source, x.reason));
}

template <class Processor>
void serialize(Processor& proc, down_msg& x, const unsigned int) {
  proc & x.source;
  proc & x.reason;
}

/// Sent to all members of a group when it goes offline.
struct group_down_msg {
  /// The source of this message, i.e., the now unreachable group.
  group source;
};

inline std::string to_string(const group_down_msg& x) {
  return "group_down" + deep_to_string(std::tie(x.source));
}

/// @relates group_down_msg
template <class Processor>
void serialize(Processor& proc, group_down_msg& x, const unsigned int) {
  proc & x.source;
}

/// Sent whenever a timeout occurs during a synchronous send.
/// This system message does not have any fields, because the message ID
/// sent alongside this message identifies the matching request that timed out.
class sync_timeout_msg { };

inline std::string to_string(const sync_timeout_msg&) {
  return "sync_timeout";
}

/// @relates group_down_msg
template <class Processor>
void serialize(Processor&, sync_timeout_msg&, const unsigned int) {
  // nop
}

/// Signalizes a timeout event.
/// @note This message is handled implicitly by the runtime system.
struct timeout_msg {
  /// Actor-specific timeout ID.
  uint32_t timeout_id;
};

inline std::string to_string(const timeout_msg& x) {
  return "timeout" + deep_to_string(std::tie(x.timeout_id));
}

/// @relates timeout_msg
template <class Processor>
void serialize(Processor& proc, timeout_msg& x, const unsigned int) {
  proc & x.timeout_id;
}

} // namespace caf

#endif // CAF_SYSTEM_MESSAGES_HPP
