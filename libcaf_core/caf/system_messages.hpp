// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/async/batch.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

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

/// Asks a source to add another sink.
/// @note The sender is always the sink.
struct stream_open_msg {
  /// The ID of the requested stream.
  uint64_t id;

  /// A handle to the new sink.
  strong_actor_ptr sink;

  /// The ID of the flow at the sink.
  uint64_t sink_flow_id;
};

/// @relates stream_open_msg
template <class Inspector>
bool inspect(Inspector& f, stream_open_msg& msg) {
  return f.object(msg).fields(f.field("id", msg.id), f.field("sink", msg.sink),
                              f.field("sink-flow-id", msg.sink_flow_id));
}

/// Asks the source for more data.
/// @note The sender is always the sink.
struct stream_demand_msg {
  /// The ID of the flow at the source.
  uint64_t source_flow_id;

  /// Additional demand from the sink.
  uint32_t demand;
};

/// @relates stream_demand_msg
template <class Inspector>
bool inspect(Inspector& f, stream_demand_msg& msg) {
  return f.object(msg).fields(f.field("source-flow-id", msg.source_flow_id),
                              f.field("demand", msg.demand));
}

/// Informs the source that the sender is no longer interest in receiving
/// items from this stream.
/// @note The sender is always the sink.
struct stream_cancel_msg {
  /// The ID of the flow at the source.
  uint64_t source_flow_id;
};

/// @relates stream_cancel_msg
template <class Inspector>
bool inspect(Inspector& f, stream_cancel_msg& msg) {
  return f.object(msg).fields(f.field("source-flow-id", msg.source_flow_id));
}

/// Informs the sink that the source has added it to the flow.
/// @note The sender is always the source.
struct stream_ack_msg {
  /// Pointer to the source actor.
  strong_actor_ptr source;

  /// The ID of the flow at the sink.
  uint64_t sink_flow_id;

  /// The ID of the flow at the source.
  uint64_t source_flow_id;

  /// Maximum amounts of items per batch.
  uint32_t max_items_per_batch;
};

/// @relates stream_ack_msg
template <class Inspector>
bool inspect(Inspector& f, stream_ack_msg& msg) {
  return f.object(msg).fields(
    f.field("source", msg.source), f.field("sink-flow-id", msg.sink_flow_id),
    f.field("source-flow-id", msg.source_flow_id),
    f.field("max-items-per-batch", msg.max_items_per_batch));
}

/// Transfers items from a source to a sink.
/// @note The sender is always the source.
struct stream_batch_msg {
  /// The ID of the flow at the sink.
  uint64_t sink_flow_id;

  /// Contains the new items from the source.
  async::batch content;
};

/// @relates stream_batch_msg
template <class Inspector>
bool inspect(Inspector& f, stream_batch_msg& msg) {
  return f.object(msg).fields(f.field("sink-flow-id", msg.sink_flow_id),
                              f.field("content", msg.content));
}

/// Informs the sink that a stream has reached the end.
/// @note The sender is always the source.
struct stream_close_msg {
  /// The ID of the flow at the sink.
  uint64_t sink_flow_id;
};

/// @relates stream_close_msg
template <class Inspector>
bool inspect(Inspector& f, stream_close_msg& msg) {
  return f.object(msg).fields(f.field("sink-flow-id", msg.sink_flow_id));
}

/// Informs the sink that a stream has been aborted due to an unrecoverable
/// error.
/// @note The sender is always the source.
struct stream_abort_msg {
  /// The ID of the flow at the sink.
  uint64_t sink_flow_id;

  /// Contains details about the abort reason.
  error reason;
};

/// @relates stream_abort_msg
template <class Inspector>
bool inspect(Inspector& f, stream_abort_msg& msg) {
  return f.object(msg).fields(f.field("sink-flow-id", msg.sink_flow_id),
                              f.field("reason", msg.reason));
}

} // namespace caf
