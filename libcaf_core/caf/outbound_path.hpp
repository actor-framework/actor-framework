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
#include "caf/logger.hpp"
#include "caf/stream_aborter.hpp"
#include "caf/stream_slot.hpp"
#include "caf/system_messages.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// State for a single path to a sink of a `downstream_manager`.
class outbound_path {
public:
  // -- member types -----------------------------------------------------------

  /// Propagates graceful shutdowns.
  using regular_shutdown = downstream_msg::close;

  /// Propagates errors.
  using irregular_shutdown = downstream_msg::forced_close;

  /// Stores batches until receiving corresponding ACKs.
  using cache_type = std::deque<std::pair<int64_t, downstream_msg::batch>>;

  // -- nested classes ---------------------------------------------------------

  /// Stores information about the initiator of the steam.
  struct client_data {
    strong_actor_ptr hdl;
    message_id mid;
  };

  // -- constants --------------------------------------------------------------

  /// Stream aborter flag to monitor a path.
  static constexpr const auto aborter_type = stream_aborter::sink_aborter;

  // -- constructors, destructors, and assignment operators --------------------

  /// Constructs a pending path for given slot and handle.
  outbound_path(stream_slot sender_slot, strong_actor_ptr receiver_hdl);

  ~outbound_path();

  // -- downstream communication -----------------------------------------------

  /// Sends a stream handshake.
  static void emit_open(local_actor* self, stream_slot slot,
                        strong_actor_ptr to, message handshake_data,
                        stream_priority prio);

  /// Sends a `stream_msg::batch` on this path, decrements `open_credit` by
  /// Sets `open_credit` to `initial_credit` and clears `cached_handshake`.
  void handle_ack_open(long initial_credit);

  /// `xs_size` and increments `next_batch_id` by 1.
  void emit_batch(local_actor* self, long xs_size, message xs);

  /// Calls `emit_batch` for each chunk in the cache, whereas each chunk is of
  /// size `desired_batch_size`. Does nothing for pending paths.
  template <class T>
  void emit_batches(local_actor* self, std::vector<T>& cache,
                    bool force_underfull) {
    CAF_LOG_TRACE(CAF_ARG(cache) << CAF_ARG(force_underfull));
    if (pending())
      return;
    CAF_ASSERT(desired_batch_size > 0);
    if (cache.size() == desired_batch_size) {
      emit_batch(self, desired_batch_size, make_message(std::move(cache)));
      return;
    }
    auto i = cache.begin();
    auto e = cache.end();
    while (std::distance(i, e) >= static_cast<ptrdiff_t>(desired_batch_size)) {
      std::vector<T> tmp{std::make_move_iterator(i),
                         std::make_move_iterator(i + desired_batch_size)};
      emit_batch(self, desired_batch_size, make_message(std::move(tmp)));
      i += desired_batch_size;
    }
    if (i == e) {
      cache.clear();
      return;
    }
    cache.erase(cache.begin(), i);
    if (force_underfull)
      emit_batch(self, cache.size(), make_message(std::move(cache)));
  }

  /// Sends a `stream_msg::drop` on this path.
  void emit_regular_shutdown(local_actor* self);

  /// Sends a `stream_msg::forced_drop` on this path.
  void emit_irregular_shutdown(local_actor* self, error reason);

  /// Sends a `stream_msg::forced_drop` on this path.
  static void emit_irregular_shutdown(local_actor* self, stream_slots slots,
                                      const strong_actor_ptr& hdl,
                                      error reason);

  // -- properties -------------------------------------------------------------

  /// Returns whether this path is pending, i.e., didn't receive an `ack_open`
  /// yet.
  inline bool pending() const noexcept {
    return slots.receiver == invalid_stream_slot;
  }

  // -- member variables -------------------------------------------------------

  /// Slot IDs for sender (self) and receiver (hdl).
  stream_slots slots;

  /// Handle to the sink.
  strong_actor_ptr hdl;

  /// Next expected batch ID.
  int64_t next_batch_id;

  /// Currently available credit for this path.
  long open_credit;

  /// Batch size configured by the downstream actor.
  uint64_t desired_batch_size;

  /// Next expected batch ID to be acknowledged. Actors can receive a more
  /// advanced batch ID in an ACK message, since CAF uses accumulative ACKs.
  int64_t next_ack_id;

  /// Caches the initiator of the stream (client) with the original request ID
  /// until the stream handshake is either confirmed or aborted. Once
  /// confirmed, the next stage takes responsibility for answering to the
  /// client.
  client_data cd;

  /// Stores whether an error occurred during stream processing.
  error shutdown_reason;
};

/// @relates outbound_path::client_data
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        outbound_path::client_data& x) {
  return f(x.hdl, x.mid);
}

/// @relates outbound_path
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, outbound_path& x) {
  return f(meta::type_name("outbound_path"), x.slots, x.hdl, x.next_batch_id,
           x.open_credit, x.desired_batch_size, x.next_ack_id, x.cd,
           x.shutdown_reason);
}

} // namespace caf

#endif // CAF_OUTBOUND_PATH_HPP
