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

#ifndef CAF_OUTBOUND_PATH_HPP
#define CAF_OUTBOUND_PATH_HPP

#include <deque>
#include <vector>
#include <cstdint>
#include <cstddef>

#include "caf/actor_control_block.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/fwd.hpp"
#include "caf/stream_aborter.hpp"
#include "caf/stream_slot.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// State for a single path to a sink on a `stream_scatterer`.
class outbound_path {
public:
  /// Stream aborter flag to monitor a path.
  static constexpr const auto aborter_type = stream_aborter::sink_aborter;

  /// Message type for propagating graceful shutdowns.
  using regular_shutdown = downstream_msg::close;

  /// Message type for propagating errors.
  using irregular_shutdown = downstream_msg::forced_close;

  /// Stores information about the initiator of the steam.
  struct client_data {
    strong_actor_ptr hdl;
    message_id mid;
  };

  /// Slot ID for the sink.
  stream_slot slot;

  /// Pointer to the parent actor.
  local_actor* self;

  /// Handle to the sink.
  strong_actor_ptr hdl;

  /// Next expected batch ID.
  int64_t next_batch_id;

  /// Currently available credit for this path.
  long open_credit;

  /// Batch size configured by the downstream actor.
  long desired_batch_size;

  /// Stores whether the downstream actor is failsafe, i.e., allows the runtime
  /// to redeploy it on failure. If this field is set to `false` then
  /// `unacknowledged_batches` is unused.
  bool redeployable;

  /// Next expected batch ID to be acknowledged. Actors can receive a more
  /// advanced batch ID in an ACK message, since CAF uses accumulative ACKs.
  int64_t next_ack_id;

  /// Caches batches until receiving an ACK.
  std::deque<std::pair<int64_t, downstream_msg::batch>> unacknowledged_batches;

  /// Caches the initiator of the stream (client) with the original request ID
  /// until the stream handshake is either confirmed or aborted. Once
  /// confirmed, the next stage takes responsibility for answering to the
  /// client.
  client_data cd;

  /// Stores whether an error occurred during stream processing.
  error shutdown_reason;

  /// Constructs a path for given handle and stream ID.
  outbound_path(local_actor* selfptr, stream_slot id,
                strong_actor_ptr ptr);

  ~outbound_path();

  /// Sets `open_credit` to `initial_credit` and clears `cached_handshake`.
  void handle_ack_open(long initial_credit);

  void emit_open(strong_actor_ptr origin,
                 mailbox_element::forwarding_stack stages, message_id mid,
                 message handshake_data, stream_priority prio,
                 bool is_redeployable);

  /// Emits a `stream_msg::batch` on this path, decrements `open_credit` by
  /// `xs_size` and increments `next_batch_id` by 1.
  void emit_batch(long xs_size, message xs);

  static void emit_irregular_shutdown(local_actor* self, stream_slot slot,
                                      const strong_actor_ptr& hdl,
                                      error reason);
};

/// @relates outbound_path
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, outbound_path& x) {
  return f(meta::type_name("outbound_path"), x.hdl, x.slot, x.next_batch_id,
           x.open_credit, x.redeployable, x.unacknowledged_batches);
}

} // namespace caf

#endif // CAF_OUTBOUND_PATH_HPP
