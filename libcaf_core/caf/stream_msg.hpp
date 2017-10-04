/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
  /// Initiates a stream handshake.
  struct open {
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Contains a type-erased stream<T> object as first argument followed by
    /// any number of user-defined additional handshake data.
    message msg;
    /// Identifies the previous stage in the pipeline.
    strong_actor_ptr prev_stage;
    /// Identifies the original receiver of this message.
    strong_actor_ptr original_stage;
    /// Configures the priority for stream elements.
    stream_priority priority;
    /// Tells the downstream whether rebindings can occur on this path.
    bool redeployable;
  };

  /// Acknowledges a previous `open` message and finalizes a stream handshake.
  /// Also signalizes initial demand.
  struct ack_open {
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Allows actors to participate in a stream instead of the actor
    /// originally receiving the `open` message. No effect when set to
    /// `nullptr`. This mechanism enables pipeline definitions consisting of
    /// proxy actors that are replaced with actual actors on demand.
    actor_addr rebind_from;
    /// Points to sender_, but with a strong reference.
    strong_actor_ptr rebind_to;
    /// Grants credit to the source.
    int32_t initial_demand;
    /// Tells the upstream whether rebindings can occur on this path.
    bool redeployable;
  };

  /// Transmits stream data.
  struct batch {
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
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Newly available credit.
    int32_t new_capacity;
    /// Cumulative ack ID.
    int64_t acknowledged_id;
  };

  /// Orderly shuts down a stream after receiving an ACK for the last batch.
  struct close {
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
  };

  /// Informs a source that a sink orderly drops out of a stream.
  struct drop {
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
  };

  /// Propagates a fatal error from sources to sinks.
  struct forced_close {
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Reason for shutting down the stream.
    error reason;
  };

  /// Propagates a fatal error from sinks to sources.
  struct forced_drop {
    /// Allows the testing DSL to unbox this type automagically.
    using outer_type = stream_msg;
    /// Reason for shutting down the stream.
    error reason;
  };

  /// Lists all possible options for the payload.
  using content_alternatives =
    detail::type_list<open, ack_open, batch, ack_batch, close, drop,
                      forced_close, forced_drop>;

  /// Stores one of `content_alternatives`.
  using content_type = variant<open, ack_open, batch, ack_batch, close, drop,
                               forced_close, forced_drop>;

  /// ID of the affected stream.
  stream_id sid;

  /// Address of the sender. Identifies the up- or downstream actor sending
  /// this message. Note that abort messages can get send after `sender`
  /// already terminated. Hence, `current_sender()` can be `nullptr`, because
  /// no strong pointers can be formed any more and receiver would receive an
  /// anonymous message.
  actor_addr sender;

  /// Palyoad of the message.
  content_type content;

  template <class T>
  stream_msg(const stream_id& id, actor_addr addr, T&& x)
      : sid(std::move(id)),
        sender(std::move(addr)),
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
make(const stream_id& sid, actor_addr addr, Ts&&... xs) {
  return {sid, std::move(addr), T{std::forward<Ts>(xs)...}};
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::open& x) {
  return f(meta::type_name("open"), x.msg, x.prev_stage, x.original_stage,
           x.priority, x.redeployable);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg::ack_open& x) {
  return f(meta::type_name("ack_open"), x.rebind_from, x.rebind_to,
           x.initial_demand, x.redeployable);
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
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::close&) {
  return f(meta::type_name("close"));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::drop&) {
  return f(meta::type_name("drop"));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::forced_close& x) {
  return f(meta::type_name("forced_close"), x.reason);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        stream_msg::forced_drop& x) {
  return f(meta::type_name("forced_drop"), x.reason);
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, stream_msg& x) {
  return f(meta::type_name("stream_msg"), x.sid, x.sender, x.content);
}

} // namespace caf

#endif // CAF_STREAM_MSG_HPP
