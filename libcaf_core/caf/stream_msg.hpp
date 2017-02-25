/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_STREAM_MSG_HPP
#define CAF_STREAM_MSG_HPP

#include <utility>
#include <vector>
#include <cstdint>

#include "caf/atom.hpp"
#include "caf/message.hpp"
#include "caf/variant.hpp"
#include "caf/stream_id.hpp"
#include "caf/stream_priority.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/tag/boxing_type.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

/// Stream communication messages for handshaking, ACKing, data transmission,
/// etc.
struct stream_msg : tag::boxing_type {
  /// A flow label characterizes nested types.
  enum flow_label {
    /// Identifies content types that only flow downstream.
    flows_downstream,
    /// Identifies content types that only flow upstream.
    flows_upstream,
    /// Identifies content types that propagate errors in both directions.
    flows_both_ways
  };

  /// Initiates a stream handshake.
  struct open {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_downstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Contains a type-erased stream<T> object as first argument followed by
    /// any number of user-defined additional handshake data.
    message msg;
    /// A pointer to the previous stage in the pipeline.
    strong_actor_ptr prev_stage;
    /// Configures the priority for stream elements.
    stream_priority priority;
    /// Tells the downstream whether rebindings can occur on this path.
    bool redeployable;
  };

  /// Acknowledges a previous `open` message and finalizes a stream handshake.
  /// Also signalizes initial demand.
  struct ack_open {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_upstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Grants credit to the source.
    int32_t initial_demand;
    /// Tells the upstream whether rebindings can occur on this path.
    bool redeployable;
  };

  /// Transmits stream data.
  struct batch {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_downstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Size of the type-erased vector<T> (used credit).
    int32_t xs_size;
    /// A type-erased vector<T> containing the elements of the batch.
    message xs;
    /// ID of this batch (ascending numbering).
    int64_t id;
  };

  /// Cumulatively acknowledges received batches and signalizes new demand from
  /// a sink to its source.
  struct ack_batch {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_upstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Newly available credit.
    int32_t new_capacity;
    /// Cumulative ack ID.
    int64_t acknowledged_id;
  };

  /// Closes a stream after receiving an ACK for the last batch.
  struct close {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_downstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
  };

  /// Propagates fatal errors.
  struct abort {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_both_ways;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Reason for shutting down the stream.
    error reason;
  };

  /// Send by the runtime if a downstream path failed. The receiving actor
  /// awaits a `resume` message afterwards if the downstream path was
  /// redeployable. Otherwise, this results in a fatal error.
  struct downstream_failed {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_upstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Exit reason of the failing downstream path.
    error reason;
  };

  /// Send by the runtime if an upstream path failed. The receiving actor
  /// awaits a `resume` message afterwards if the upstream path was
  /// redeployable. Otherwise, this results in a fatal error.
  struct upstream_failed {
    /// Allows visitors to dispatch on this tag.
    static constexpr flow_label label = flows_downstream;
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Exit reason of the failing upstream path.
    error reason;
  };

  /// Lists all possible options for the payload.
  using content_alternatives = detail::type_list<open, ack_open, batch,
                                                 ack_batch, close, abort,
                                                 downstream_failed,
                                                 upstream_failed>;

  /// Stores one of `content_alternatives`.
  using content_type = variant<open, ack_open, batch, ack_batch, close,
                               abort, downstream_failed, upstream_failed>;

  /// ID of the affected stream.
  stream_id sid;

  /// Palyoad of the message.
  content_type content;

  template <class T>
  stream_msg(stream_id  id, T&& x)
      : sid(std::move(id)),
        content(std::forward<T>(x)) {
    // nop
  }

  stream_msg() = default;
  stream_msg(stream_msg&&) = default;
  stream_msg(const stream_msg&) = default;
  stream_msg& operator=(stream_msg&&) = default;
  stream_msg& operator=(const stream_msg&) = default;
};

/// Allows the testing DSL to unbox `stream_msg` automagically.
template <class T>
const T& get(const stream_msg& x) {
  return get<T>(x.content);
}

/// Allows the testing DSL to check whether `stream_msg` holds a `T`.
template <class T>
bool is(const stream_msg& x) {
  return holds_alternative<T>(x.content);
}

template <class T, class... Ts>
typename std::enable_if<
  detail::tl_contains<
    stream_msg::content_alternatives,
    T
  >::value,
  stream_msg
>::type
make(const stream_id& sid, Ts&&... xs) {
  return {sid, T{std::forward<Ts>(xs)...}};
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::open& x) {
  return f(meta::type_name("open"), x.msg, x.prev_stage, x.priority,
           x.redeployable);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::ack_open& x) {
  return f(meta::type_name("ack_open"), x.initial_demand, x.redeployable);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::batch& x) {
  return f(meta::type_name("batch"), meta::omittable(), x.xs_size, x.xs, x.id);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::ack_batch& x) {
  return f(meta::type_name("ack_batch"), x.new_capacity, x.acknowledged_id);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::close&) {
  return f(meta::type_name("close"));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::abort& x) {
  return f(meta::type_name("abort"), x.reason);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::downstream_failed& x) {
  return f(meta::type_name("downstream_failed"), x.reason);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::upstream_failed& x) {
  return f(meta::type_name("upstream_failed"), x.reason);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg& x) {
  return f(meta::type_name("stream_msg"), x.sid, x.content);
}

} // namespace caf

#endif // CAF_STREAM_MSG_HPP
