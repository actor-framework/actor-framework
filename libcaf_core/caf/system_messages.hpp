// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/fwd.hpp"
#include "caf/group.hpp"

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
inline bool operator==(const exit_msg& x, const exit_msg& y) noexcept {
  return x.source == y.source && x.reason == y.reason;
}

/// @relates exit_msg
template <class Inspector>
bool inspect(Inspector& f, exit_msg& x) {
  return f.object(x).fields(f.field("source", x.source),
                            f.field("reason", x.reason));
}

/// Sent to all actors monitoring an actor when it is terminated.
struct down_msg {
  /// The source of this message, i.e., the terminated actor.
  actor_addr source;

  /// The exit reason of the terminated actor.
  error reason;
};

/// @relates down_msg
inline bool operator==(const down_msg& x, const down_msg& y) noexcept {
  return x.source == y.source && x.reason == y.reason;
}

/// @relates down_msg
inline bool operator!=(const down_msg& x, const down_msg& y) noexcept {
  return !(x == y);
}

/// @relates down_msg
template <class Inspector>
bool inspect(Inspector& f, down_msg& x) {
  return f.object(x).fields(f.field("source", x.source),
                            f.field("reason", x.reason));
}

/// Sent to all members of a group when it goes offline.
struct group_down_msg {
  /// The source of this message, i.e., the now unreachable group.
  group source;
};

/// @relates group_down_msg
template <class Inspector>
bool inspect(Inspector& f, group_down_msg& x) {
  return f.object(x).fields(f.field("source", x.source));
}

/// Sent to all actors monitoring a node when CAF loses connection to it.
struct node_down_msg {
  /// The disconnected node.
  node_id node;

  /// The cause for the disconnection. No error (a default-constructed error
  /// object) indicates an ordinary shutdown.
  error reason;
};

/// @relates node_down_msg
inline bool operator==(const node_down_msg& x,
                       const node_down_msg& y) noexcept {
  return x.node == y.node && x.reason == y.reason;
}

/// @relates node_down_msg
inline bool operator!=(const node_down_msg& x,
                       const node_down_msg& y) noexcept {
  return !(x == y);
}

/// @relates node_down_msg
template <class Inspector>
bool inspect(Inspector& f, node_down_msg& x) {
  return f.object(x).fields(f.field("node", x.node),
                            f.field("reason", x.reason));
}

} // namespace caf
