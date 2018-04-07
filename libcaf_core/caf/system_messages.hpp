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

#include <vector>
#include <cstdint>
#include <type_traits>

#include "caf/actor_addr.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/fwd.hpp"
#include "caf/group.hpp"
#include "caf/stream_slot.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// Sent to all links when an actor is terminated.
/// @note Actors can override the default handler by calling
///       `self->set_exit_handler(...)`.
struct exit_msg {
  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;
};

/// @relates exit_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, exit_msg& x) {
  return f(meta::type_name("exit_msg"), x.source, x.reason);
}

/// Sent to all actors monitoring an actor when it is terminated.
struct down_msg {
  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;
};

/// @relates down_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, down_msg& x) {
  return f(meta::type_name("down_msg"), x.source, x.reason);
}

/// Sent to all members of a group when it goes offline.
struct group_down_msg {
  /// The source of this message, i.e., the now unreachable group.
  group source;
};

/// @relates group_down_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, group_down_msg& x) {
  return f(meta::type_name("group_down_msg"), x.source);
}

/// Signalizes a timeout event.
/// @note This message is handled implicitly by the runtime system.
struct timeout_msg {
  /// Type of the timeout (either `receive_atom` or `cycle_atom`).
  atom_value type;
  /// Actor-specific timeout ID.
  uint64_t timeout_id;
};

/// @relates timeout_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, timeout_msg& x) {
  return f(meta::type_name("timeout_msg"), x.type, x.timeout_id);
}

/// Demands the receiver to open a new stream from the sender to the receiver.
struct open_stream_msg {
  /// Reserved slot on the source.
  stream_slot slot;

  /// Contains a type-erased stream<T> object as first argument followed by
  /// any number of user-defined additional handshake data.
  message msg;

  /// Identifies the previous stage in the pipeline.
  strong_actor_ptr prev_stage;

  /// Identifies the original receiver of this message.
  strong_actor_ptr original_stage;

  /// Configures the priority for stream elements.
  stream_priority priority;
};

/// @relates open_stream_msg
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, open_stream_msg& x) {
  return f(meta::type_name("open_stream_msg"), x.slot, x.msg, x.prev_stage,
           x.original_stage, x.priority);
}

} // namespace caf

